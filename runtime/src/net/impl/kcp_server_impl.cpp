#include "kcp_server_impl.h"

#include <simple/error.h>
#include <simple/log/log.h>
#include <simple/net/socket_system.h>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <ranges>

#include "kcp_config.h"
#include "kcp_session_impl.h"

namespace simple {

kcp_server_impl::kcp_server_impl(uint32_t socket_id) : socket_base(socket_id), listen_(socket_system::instance().context()) {}

void kcp_server_impl::start(const udp::endpoint& endpoint, bool reuse) {
    info("kcp server {} start", socket_id_);
    std::error_code ec;
    listen_.open(endpoint.protocol(), ec);
    if (ec) return stop(ec);
    listen_.bind(endpoint, ec);
    if (ec) return stop(ec);
    listen_.set_option(udp::socket::reuse_address{reuse}, ec);
    if (ec) return stop(ec);

    auto self = shared_from_this();
    auto& system = socket_system::instance();
    system.insert(socket_id_, self);
    system.hand_start(socket_id_);

    co_spawn(
        system.context(),
        [self, this]() {
            std::ignore = self;
            return co_read();
        },
        asio::detached);
}

constexpr kcp_server_impl::asio_token use_awaitable_as_tuple;

void kcp_server_impl::stop(const std::error_code& ec) {
    if (!listen_.is_open()) return;

    info("kcp server {} stop", socket_id_);

    for (auto sessions = std::move(sessions_); const auto& session : sessions | std::views::values) {
        session->stop(ec);
    }

    std::error_code ignore;
    listen_.close(ignore);
    auto& system = socket_system::instance();
    system.hand_stop(socket_id_, ec);
    system.erase(socket_id_);
}

void kcp_server_impl::erase(uint32_t session) { sessions_.erase(session); }

void kcp_server_impl::write_to(const udp::endpoint& dest, std::vector<uint8_t> data) {
    auto self = shared_from_this();
    const auto msg = std::make_shared<std::vector<uint8_t>>(std::move(data));
    listen_.async_send_to(asio::buffer(msg->data(), msg->size()), dest, [self, msg](const std::error_code&, size_t) {
        std::ignore = self;
        std::ignore = msg;
    });
}

asio::awaitable<void> kcp_server_impl::co_read() {
    auto self = shared_from_this();
    uint8_t data[udp_mtu];
    for (;;) {
        udp::endpoint remote_endpoint;
        auto [ec, len] = co_await listen_.async_receive_from(asio::buffer(data), remote_endpoint, use_awaitable_as_tuple);
        if (ec) {
            stop(ec);
            co_return;
        }

        if (len < kcp_head_size) {
            continue;
        }

        const auto* head = reinterpret_cast<kcp_head*>(data);
        if (head->magic1 != kcp_magic1 || head->magic2 != kcp_magic2 || head->magic3 != kcp_magic3) {
            continue;
        }

        // hand data
        if (head->code == kcp_code::connect) {
            hand_accept(std::move(remote_endpoint));
            continue;
        }

        uint32_t conv;
        if (head->code == kcp_code::data) {
            conv = ikcp_getconv(data + kcp_head_size);
        } else {
            if (len < kcp_head_size + sizeof(uint32_t)) {
                continue;
            }
            memcpy(&conv, data + kcp_head_size, sizeof(conv));
            conv = ntohl(conv);
        }

        if (const auto it = sessions_.find(conv); it != sessions_.end()) {
            it->second->read(data, len);
        }
    }
}

void kcp_server_impl::hand_accept(udp::endpoint remote) {
    auto& system = socket_system::instance();
    const auto id = system.new_socket_id(socket_type::kcp_session);
    if (id == 0) {
        warn("kcp server {} accept fail, no new socket id", socket_id_);
        write_to(remote, make_kcp_ctrl(kcp_code::disconnect, 0));
        return;
    }

    trace_read(1);
    const auto session = std::make_shared<kcp_session_impl>(id, std::move(remote), *this);
    sessions_[id] = session.get();
    session->start(socket_id_);
}

}  // namespace simple
