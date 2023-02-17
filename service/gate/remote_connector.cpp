﻿#include "remote_connector.h"

#include <proto_utils.h>
#include <simple/coro/network.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>

#include "gate.h"

remote_connector::remote_connector(const remote_gate* remote, gate& g) : remote_(remote), gate_(g) {}

void remote_connector::start() {
    simple::co_start([this]() { return run(); });
}

void remote_connector::send(simple::memory_buffer_ptr ptr) {
    send_queue_.emplace_back(std::move(ptr));
    // 可能会有上一个连接的发送协程在等待
    cv_send_queue_.notify_all();
}

simple::task<> remote_connector::run() {
    using namespace std::chrono_literals;
    auto& network = simple::network::instance();
    size_t address_cnt = 0;
    for (;;) {
        const auto address_size = remote_->addresses.size();
        if (address_size == 0) {
            simple::warn("[{}] remote:{} addresses is empty.", gate_.name(), remote_->id);
            co_await simple::sleep_for(2s);
            continue;
        }

        const auto& address = remote_->addresses[address_cnt % address_size];
        simple::warn("[{}] remote:{} address:{}", gate_.name(), remote_->id, address);

        const auto pos = address.find(',');
        if (pos == std::string::npos || pos + 1 == address.size()) {
            ++address_cnt;
            simple::warn("[{}] remote:{} address:{} is invalid.", gate_.name(), remote_->id, address);
            continue;
        }

        const auto host = address.substr(0, pos);
        const auto port = address.substr(pos + 1);

        try {
            socket_ = co_await network.tcp_connect(host, port, 10s);
            if (socket_ == 0) {
                ++address_cnt;
                simple::error("[{}] remote:{} address:{} tcp_connect fail", gate_.name(), remote_->id, address);
                continue;
            }

            simple::warn("[{}] connect remote:{} address:{} succ.", gate_.name(), remote_->id, address);

            // 开启ping协程
            simple::co_start([this, socket = socket_]() { return auto_ping(socket); });
            // 开启发送协程
            simple::co_start([this, socket = socket_]() { return auto_send(socket); });
            // 循环接收消息
            simple::memory_buffer buffer;
            net_header header{};
            for (;;) {
                co_await recv_net_buffer(buffer, header, socket_);
                const uint32_t len = header.len;
                simple::info("[{}] remote:{} socket:{} recv id:{} session:{} len:{}", gate_.name(), remote_->id, socket_,
                             header.id, header.session, len);
                // 只会收到ping的ack
                system_.wake_up_session(header.session, std::string_view(buffer));
            }
        } catch (std::exception& e) {
            socket_ = 0;
            network.close(socket_);
            simple::error("[{}] remote:{} exception {}", gate_.name(), remote_->id, ERROR_CODE_MESSAGE(e.what()));
        }
    }
}

simple::task<> remote_connector::auto_ping(uint32_t socket) {
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

simple::task<> remote_connector::auto_send(uint32_t socket) {
    auto& network = simple::network::instance();
    while (socket == socket_) {
        for (const auto& ptr : send_queue_) {
            network.write(socket, ptr);
        }
        send_queue_.clear();
        co_await cv_send_queue_.wait();
    }
}

simple::task<> remote_connector::ping_to_remote(uint32_t socket) {
    const auto ping = co_await rpc_ping(system_, socket);
    simple::info("[{}] remote:{} ping delay:{}ms", gate_.name(), remote_->id, ping);
}
