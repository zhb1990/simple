#include "remote_gate.h"

#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <proto_utils.h>
#include <simple/coro/network.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <simple/application/service.hpp>
#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>

void remote_service::write(const std::string_view& message) {
    net_header header{flag_valid, 0, static_cast<uint16_t>(game::id_s_gate_forward_brd), 0, 0};
    header.len = static_cast<uint32_t>(message.size());
    auto buf = std::make_shared<simple::memory_buffer>();
    buf->reserve(header.len + sizeof(header));
    buf->append(&header, sizeof(header));
    buf->append(message.data(), message.size());
    remote->send(buf);
}

remote_gate::remote_gate(simple::service& s, uint16_t id) : service_(&s), id_(id) {}

void remote_gate::start() {
    simple::co_start([this]() { return run(); });
}

void remote_gate::send(simple::memory_buffer_ptr ptr) {
    if (socket_ > 0) {
        simple::network::instance().write(socket_, ptr);
    } else {
        send_queue_.emplace_back(std::move(ptr));
    }
}

void remote_gate::set_addresses(std::vector<std::string> addresses) { addresses_ = std::move(addresses); }

simple::task<> remote_gate::run() {
    using namespace std::chrono_literals;
    auto& network = simple::network::instance();
    size_t address_cnt = 0;
    size_t address_size = 0;
    size_t cnt_fail = 0;
    auto inc_address_cnt = [&] {
        ++address_cnt;
        if (address_cnt % address_size == 0) {
            ++cnt_fail;
        }
    };

    for (;;) {
        address_size = addresses_.size();
        if (address_size == 0) {
            simple::warn("[{}] remote:{} addresses is empty.", service_->name(), id_);
            co_await simple::sleep_for(2s);
            continue;
        }

        const auto& address = addresses_[address_cnt % address_size];
        simple::warn("[{}] remote:{} address:{}", service_->name(), id_, address);

        const auto pos = address.find(',');
        if (pos == std::string::npos || pos + 1 == address.size()) {
            inc_address_cnt();
            simple::warn("[{}] remote:{} address:{} is invalid.", service_->name(), id_, address);
            continue;
        }

        const auto host = address.substr(0, pos);
        const auto port = address.substr(pos + 1);

        try {
            if (const auto interval = connect_interval(cnt_fail); interval > 0) {
                co_await simple::sleep_for(std::chrono::milliseconds(interval));
            }

            socket_ = co_await network.tcp_connect(host, port, 10s);
            if (socket_ == 0) {
                inc_address_cnt();
                simple::error("[{}] remote:{} address:{} tcp_connect fail", service_->name(), id_, address);
                continue;
            }

            cnt_fail = 0;
            simple::warn("[{}] connect remote:{} address:{} succ.", service_->name(), id_, address);

            // 发送断网过程中的消息
            auto_send(socket_);
            // 开启ping协程
            simple::co_start([this, socket = socket_]() { return auto_ping(socket); });
            // 循环接收消息
            simple::memory_buffer buffer;
            net_header header{};
            for (;;) {
                co_await recv_net_buffer(buffer, header, socket_);
                const uint32_t len = header.len;
                simple::info("[{}] remote:{} socket:{} recv id:{} session:{} len:{}", service_->name(), id_, socket_, header.id,
                             header.session, len);
                // 只会收到ping的ack
                system_.wake_up_session(header.session, std::string_view(buffer));
            }
        } catch (std::exception& e) {
            network.close(socket_);
            socket_ = 0;
            simple::error("[{}] remote:{} exception {}", service_->name(), id_, ERROR_CODE_MESSAGE(e.what()));
        }
    }
}

simple::task<> remote_gate::auto_ping(uint32_t socket) {
    auto& network = simple::network::instance();
    co_await simple::sleep_for(std::chrono::seconds(20));
    while (socket == socket_) {
        auto timeout = []() -> simple::task<> { co_await simple::sleep_for(std::chrono::seconds(5)); };
        auto result = co_await (ping_to_remote(socket) || timeout());
        if (result.index() == 0) {
            co_await simple::sleep_for(std::chrono::seconds(20));
            continue;
        }

        if (socket == socket_) {
            network.close(socket);
        }
        break;
    }
}

void remote_gate::auto_send(uint32_t socket) {
    auto& network = simple::network::instance();
    for (const auto& ptr : send_queue_) {
        network.write(socket, ptr);
    }
    send_queue_.clear();
}

simple::task<> remote_gate::ping_to_remote(uint32_t socket) {
    const auto ping = co_await rpc_ping(system_, socket);
    simple::info("[{}] remote:{} ping delay:{}ms", service_->name(), id_, ping);
}

void remote_gate::add_remote_service(const game::s_service_info& info) {
    try {
        const auto id = static_cast<uint16_t>(info.id());
        auto& router = service_->router();
        auto* service_ptr = router.call<service_info*>("find_service", id);
        if (!service_ptr) {
            auto& new_service = services_.emplace_back();
            new_service.remote = this;
            new_service.gate = id_;
            new_service.id = id;
            new_service.tp = info.tp();
            new_service.service = service_;
            router.call("emplace_service", dynamic_cast<service_info*>(&new_service));
            service_ptr = &new_service;
        }
        service_ptr->online = info.online();
        service_ptr->update();
    } catch (...) {
        simple::warn("catch");
    }
}
