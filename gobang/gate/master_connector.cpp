#include "master_connector.h"

#include <google/protobuf/util/json_util.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <proto_utils.h>
#include <simple/coro/network.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>

#include "gate.h"

master_connector::master_connector(gate& g) : gate_(g) {}

void master_connector::start() {
    simple::co_start([this]() { return run(); });
}

simple::task<int32_t> master_connector::upload_to_master(const game::s_service_info& info) {
    if (socket_ == 0) {
        co_return game::ec_system;
    }

    game::s_service_update_req req;
    *req.add_services() = info;

    const auto ack = co_await rpc_call<game::msg_common_ack>(system_, socket_, game::id_s_service_update_req, req);
    co_return ack.result().ec();
}

simple::task<> master_connector::run() {
    using namespace std::chrono_literals;
    auto& network = simple::network::instance();
    size_t cnt_fail = 0;
    for (;;) {
        auto& address = gate_.master_address();
        simple::warn("[{}] address:{}", gate_.name(), address);
        const auto pos = address.find(',');
        if (pos == std::string::npos || pos + 1 == address.size()) {
            throw std::logic_error("gate master address is invalid");
        }

        const auto host = address.substr(0, pos);
        const auto port = address.substr(pos + 1);

        try {
            if (const auto interval = connect_interval(cnt_fail); interval > 0) {
                co_await simple::sleep_for(std::chrono::milliseconds(interval));
            }

            socket_ = co_await network.tcp_connect(host, port, 10s);
            if (socket_ == 0) {
                ++cnt_fail;
                simple::error("[{}] tcp_connect fail", gate_.name());
                continue;
            }

            simple::warn("[{}] connect gate master address:{} succ.", gate_.name(), address);

            // 开启接收注册的结果协程
            simple::co_start([this, socket = socket_]() { return recv_one_message(socket); });

            // register
            auto timeout = []() -> simple::task<> { co_await simple::sleep_for(5s); };
            auto result = co_await (register_to_master(socket_) || timeout());
            if (const auto* succ = std::get_if<0>(&result); (!succ) || (!(*succ))) {
                if (succ) {
                    simple::error("[{}] register to gate master fail.", gate_.name());
                } else {
                    // 注册超时了
                    simple::error("[{}] register to gate master timeout.", gate_.name());
                }
                network.close(socket_);
                socket_ = 0;
                ++cnt_fail;
                continue;
            }

            simple::warn("[{}] register to gate master succ.", gate_.name());
            cnt_fail = 0;
            // 开启ping协程
            simple::co_start([this, socket = socket_]() { return auto_ping(socket); });

            // 循环接收消息
            simple::memory_buffer buffer;
            net_header header{};
            for (;;) {
                co_await recv_net_buffer(buffer, header, socket_);
                const uint32_t len = header.len;
                simple::info("[{}] socket:{} recv id:{} session:{} len:{}", gate_.name(), socket_, header.id, header.session,
                             len);
                if ((header.id & game::msg_mask) != game::msg_s2s_ack || header.session == 0 ||
                    (!system_.wake_up_session(header.session, std::string_view(buffer)))) {
                    // 不是rpc调用，分发消息
                    forward_message(header.id, buffer);
                }
            }
        } catch (std::exception& e) {
            network.close(socket_);
            socket_ = 0;
            ++cnt_fail;
            simple::error("[{}] exception {}", gate_.name(), ERROR_CODE_MESSAGE(e.what()));
        }
    }
}

simple::task<> master_connector::recv_one_message(uint32_t socket) {
    simple::memory_buffer buffer;
    net_header header{};
    co_await recv_net_buffer(buffer, header, socket);
    const uint32_t len = header.len;
    simple::info("[{}] socket:{} recv id:{} session:{} len:{}", gate_.name(), socket, header.id, header.session, len);
    // 第一个消息是注册的回复，一定是rpc调用
    system_.wake_up_session(header.session, std::string_view(buffer));
}

simple::task<> master_connector::auto_ping(uint32_t socket) {
    auto& network = simple::network::instance();
    while (socket == socket_) {
        co_await simple::sleep_for(std::chrono::seconds(20));
        auto timeout = []() -> simple::task<> { co_await simple::sleep_for(std::chrono::seconds(5)); };
        auto result = co_await (ping_to_master(socket) || timeout());
        if (result.index() == 0) {
            continue;
        }

        if (socket == socket_) {
            network.close(socket);
        }
        break;
    }
}

simple::task<bool> master_connector::register_to_master(uint32_t socket) {
    game::s_gate_register_req req;
    auto& info = *req.mutable_info();
    info.set_id(gate_.id());
    const auto port = fmt::format("{}", gate_.remote_port());

    for (const auto& host : gate_.remote_hosts()) {
        auto& add = *info.add_addresses();
        add.set_host(host);
        add.set_port(port);
    }

    // 填入本地的服务
    for (auto* s : gate_.local_services()) {
        s->to_proto(*info.add_services());
    }

    const auto ack = co_await rpc_call<game::s_gate_register_ack>(system_, socket, game::id_s_gate_register_req, req);
    if (auto& result = ack.result(); result.ec() != game::ec_success) {
        co_return false;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(ack, &log_str);
    simple::info("[{}] register ack:{}", gate_.name(), log_str);

    // 记录其他gate的信息
    for (auto& g : ack.gates()) {
        gate_.add_remote_gate(g);
    }

    co_return true;
}

simple::task<> master_connector::ping_to_master(uint32_t socket) {
    const auto ping = co_await rpc_ping(system_, socket);
    simple::info("[{}] ping delay:{}ms", gate_.name(), ping);
}

void master_connector::forward_message(uint16_t id, const simple::memory_buffer& buffer) const {
    // 目前只需要处理master发来的其他gate信息广播，直接if判断下
    if (id != game::id_s_gate_register_brd) {
        return;
    }

    game::s_gate_register_brd brd;
    if (!brd.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("[{}] parse s_gate_register_brd fail", gate_.name());
        return;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(brd, &log_str);
    simple::info("[{}] register brd:{}", gate_.name(), log_str);

    // 记录其他gate的信息
    for (auto& g : brd.gates()) {
        gate_.add_remote_gate(g);
    }
}
