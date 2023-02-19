#include "gate_connector.h"

#include <google/protobuf/util/json_util.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <proto_utils.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>
#include <stdexcept>

gate_connector::gate_connector(simple::service_base& service, const simple::toml_value_t* value, int32_t service_type,
                               forward_gate_fn forward_gate, forward_fn forward, shm_infos infos)
    : service_(service),
      service_type_(service_type),
      forward_gate_(std::move(forward_gate)),
      forward_(std::move(forward)),
      shm_infos_(std::move(infos)) {
    if (!value->is_table()) {
        throw std::logic_error("gate_connector need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("port"); it != args.end() && it->second.is_integer()) {
        port_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("gate_connector need gate port");
    }

    if (const auto it = args.find("channel_size"); it != args.end() && it->second.is_integer()) {
        channel_size_ = static_cast<uint32_t>(it->second.as_integer());
    } else {
        throw std::logic_error("gate_connector need channel size");
    }
}

void gate_connector::start() {
    simple::co_start([this]() { return run(); });
}

void gate_connector::write(uint16_t to, uint64_t session, uint16_t id, const google::protobuf::Message& msg) {
    if (!channel_ || !send_queue_.empty()) {
        // 通道还未建立，或者发送队列中有消息没发完，直接放入发送队列
        auto ptr = std::make_shared<simple::memory_buffer>();
        init_shm_buffer(*ptr, {service_.id(), to, id, 0, session}, msg);
        send_queue_.emplace_back(ptr);
        cv_send_queue_.notify_all();
        return;
    }

    simple::memory_buffer buf;
    init_shm_buffer(buf, {service_.id(), to, id, 0, session}, msg);
    if (!channel_->try_write(buf.begin_read(), static_cast<uint32_t>(buf.readable()))) {
        // 写入失败说明共享内存写满了，先放入发送队列
        send_queue_.emplace_back(std::make_shared<simple::memory_buffer>(std::move(buf)));
        cv_send_queue_.notify_all();
    }
}

simple::task<> gate_connector::run() {
    using namespace std::chrono_literals;
    auto& network = simple::network::instance();
    size_t cnt_fail = 0;
    auto timeout = []() -> simple::task<> { co_await simple::sleep_for(5s); };
    for (;;) {
        try {
            if (const auto interval = connect_interval(cnt_fail); interval > 0) {
                co_await simple::sleep_for(std::chrono::milliseconds(interval));
            }

            socket_ = co_await network.tcp_connect("localhost", std::to_string(port_), 10s);
            if (socket_ == 0) {
                ++cnt_fail;
                simple::error("[{}] connect gate port:{} fail", service_.name(), port_);
                continue;
            }

            simple::warn("[{}] connect gate port:{} succ.", service_.name(), port_);

            // 开启接收注册的结果协程
            simple::co_start([this, socket = socket_]() { return recv_one_message(socket); });

            // register
            auto result = co_await (register_to_gate(socket_) || timeout());
            if (const auto* succ = std::get_if<0>(&result); (!succ) || (!(*succ))) {
                if (succ) {
                    simple::error("[{}] register to gate fail.", service_.name());
                } else {
                    // 注册超时了
                    simple::error("[{}] register to gate timeout.", service_.name());
                }
                network.close(socket_);
                socket_ = 0;
                ++cnt_fail;
                continue;
            }

            simple::warn("[{}] register to gate succ.", service_.name());
            cnt_fail = 0;
            // 开启ping协程
            simple::co_start([this, socket = socket_]() { return auto_ping(socket); });

            // 循环接收消息
            simple::memory_buffer buffer;
            net_header header{};
            for (;;) {
                co_await recv_net_buffer(buffer, header, socket_);
                const uint32_t len = header.len;
                simple::info("[{}] socket:{} recv id:{} session:{} len:{}", service_.name(), socket_, header.id, header.session,
                             len);
                if ((header.id & game::msg_mask) != game::msg_s2s_ack || header.session == 0 ||
                    (!system_.wake_up_session(header.session, std::string_view(buffer)))) {
                    // 不是rpc调用，分发消息
                    forward_gate_(socket_, header.session, header.id, buffer);
                }
            }
        } catch (std::exception& e) {
            network.close(socket_);
            socket_ = 0;
            ++cnt_fail;
            simple::error("[{}] exception {}", service_.name(), ERROR_CODE_MESSAGE(e.what()));
        }
    }
}

simple::task<> gate_connector::recv_one_message(uint32_t socket) {
    simple::memory_buffer buffer;
    net_header header{};
    co_await recv_net_buffer(buffer, header, socket);
    const uint32_t len = header.len;
    simple::info("[{}] socket:{} recv id:{} session:{} len:{}", service_.name(), socket, header.id, header.session, len);
    // 第一个消息是注册的回复，一定是rpc调用
    system_.wake_up_session(header.session, std::string_view(buffer));
}

simple::task<> gate_connector::auto_ping(uint32_t socket) {
    auto& network = simple::network::instance();
    while (socket == socket_) {
        co_await simple::sleep_for(std::chrono::seconds(20));
        auto timeout = []() -> simple::task<> { co_await simple::sleep_for(std::chrono::seconds(5)); };
        auto result = co_await (ping_to_gate(socket) || timeout());
        if (result.index() == 0) {
            continue;
        }

        if (socket == socket_) {
            network.close(socket);
        }
        break;
    }
}

simple::task<> gate_connector::ping_to_gate(uint32_t socket) {
    const auto ping = co_await rpc_ping(system_, socket);
    simple::info("[{}] ping delay:{}ms", service_.name(), ping);
}

simple::task<bool> gate_connector::register_to_gate(uint32_t socket) {
    game::s_service_register_req req;
    auto& info = *req.mutable_info();
    info.set_id(service_.id());
    info.set_tp(static_cast<game::service_type>(service_type_));

    req.set_channel_size(channel_size_);

    for (const auto& [name, size] : shm_infos_) {
        auto& add = *req.add_shm();
        add.set_name(name);
        add.set_size(size);
    }

    const auto ack = co_await rpc_call<game::s_service_register_ack>(system_, socket, game::id_s_service_register_req, req);
    if (auto& result = ack.result(); result.ec() != game::ec_success) {
        co_return false;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(ack, &log_str);
    simple::info("[{}] register ack:{}", service_.name(), log_str);

    // 创建共享内存通道
    if (!channel_) {
        channel_ =
            std::make_unique<simple::shm_channel>(std::to_string(service_.id()), std::to_string(ack.gate()), channel_size_);

        simple::co_start([this]() { return channel_write(); });
        simple::co_start([this]() { return channel_read(); });
    }

    co_return true;
}

simple::task<> gate_connector::channel_read() {
    simple::memory_buffer buf;
    for (;;) {
        co_await channel_->read(buf);
        const shm_header& header = *reinterpret_cast<const shm_header*>(buf.begin_read());
        buf.read(sizeof(header));
        if ((header.id & game::msg_mask) != game::msg_s2s_ack || header.session == 0 ||
            (!system_.wake_up_session(header.session, std::string_view(buf)))) {
            // 不是rpc调用，分发消息
            forward_(header.from, header.session, header.id, buf);
        }
    }
}

simple::task<> gate_connector::channel_write() {
    for (;;) {
        if (send_queue_.empty()) {
            co_await cv_send_queue_.wait();
            continue;
        }

        const auto ptr = send_queue_.front();
        co_await channel_->write(ptr->begin_read(), static_cast<uint32_t>(ptr->readable()));
        send_queue_.pop_front();
    }
}
