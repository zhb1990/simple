#include "kcp_session_impl.h"

#include <simple/error.h>
#include <simple/log/log.h>
#include <simple/net/socket_system.h>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include "kcp_config.h"
#include "kcp_server_impl.h"

namespace simple {

constexpr kcp_session_impl::asio_token use_awaitable_as_tuple;

kcp_session_impl::kcp_session_impl(uint32_t socket_id, udp::endpoint remote, kcp_server_impl& server)  // NOLINT
    : socket_base(socket_id),
      remote_(std::move(remote)),
      server_(server),
      kcp_update_(socket_system::instance().context()),
      deadline_(socket_system::instance().context()) {}

kcp_session_impl::~kcp_session_impl() noexcept {
    if (kcp_) {
        ikcp_release(kcp_);
    }
}

void kcp_session_impl::start(uint32_t acceptor_id) {
    info("kcp session {} acceptor:{} start", socket_id_, acceptor_id);
    auto& system = socket_system::instance();
    const auto self = shared_from_this();
    system.insert(socket_id_, self);

    std::error_code ignore;
    auto local = server_.socket().local_endpoint(ignore);
    system.hand_accept(acceptor_id, socket_id_, to_string(local), to_string(remote_));

    kcp_ = kcp_create_default(socket_id_, this);
    ikcp_setoutput(kcp_, [](const char* buf, int len, ikcpcb* kcp, void* user) {
        auto* client = static_cast<kcp_session_impl*>(user);
        auto output = std::make_shared<std::vector<uint8_t>>();
        client->last_write_ = asio_timer::clock_type::now();
        client->server_.write_to(client->remote_, make_kcp_data(buf, len));
        return 0;
    });

    last_read_ = asio_timer::clock_type::now();
    last_write_ = last_read_;
    server_.write_to(remote_, make_kcp_ctrl(kcp_code::connect_ack, socket_id_));
}

void kcp_session_impl::accept() {
    auto& system = socket_system::instance();
    auto self = shared_from_this();
    auto& context = system.context();

    // 检查协程
    co_spawn(
        context,
        [self, this]() {
            std::ignore = self;
            return co_watchdog();
        },
        asio::detached);

    // kcp update协程
    co_spawn(
        context,
        [self, this]() {
            std::ignore = self;
            return co_kcp_update();
        },
        asio::detached);
}

void kcp_session_impl::stop(const std::error_code& ec) {
    if (!enable_) return;

    enable_ = false;
    info("kcp session {} stop", socket_id_);
    server_.write_to(remote_, make_kcp_ctrl(kcp_code::disconnect, socket_id_));
    server_.erase(socket_id_);

    try {
        kcp_update_.cancel();
        deadline_.cancel();
    } catch (...) {
    }

    auto& system = socket_system::instance();
    system.hand_stop(socket_id_, ec);
    system.erase(socket_id_);
}

void kcp_session_impl::write(const memory_buffer_ptr& ptr) {
    auto* data = reinterpret_cast<const char*>(ptr->begin_read());
    auto len = static_cast<int>(ptr->readable());

    while (len > kcp_recv_capacity) {
        if (ikcp_send(kcp_, data, kcp_recv_capacity) < 0) {
            return stop(socket_errors::kcp_protocol_error);
        }
        len -= kcp_recv_capacity;
        data += kcp_recv_capacity;
    }

    if (len > 0 && ikcp_send(kcp_, data, len) < 0) {
        return stop(socket_errors::kcp_protocol_error);
    }

    force_update_ = true;
}

void kcp_session_impl::no_delay(bool on) {
    if (kcp_) {
        kcp_no_delay(kcp_, on);
    }
}

void kcp_session_impl::read(uint8_t* data, size_t len) {
    const auto* head = reinterpret_cast<kcp_head*>(data);
    trace_read(len);
    switch (head->code) {  // NOLINT(clang-diagnostic-switch-enum)
        case kcp_code::disconnect:
            return stop(asio::error::eof);
        case kcp_code::heartbeat:
            last_write_ = asio_timer::clock_type::now();
            server_.write_to(remote_, make_kcp_ctrl(kcp_code::heartbeat_ack, socket_id_));
            break;
        case kcp_code::heartbeat_ack:
            break;
        case kcp_code::data:
            if (!hand_read_data(data + kcp_head_size, len - kcp_head_size)) {
                return;
            }
            break;
        default:
            return;
    }

    last_read_ = asio_timer::clock_type::now();
}

asio::awaitable<void> kcp_session_impl::co_watchdog() {
    auto self = shared_from_this();
    auto now = std::chrono::steady_clock::now();
    auto deadline_point = last_read_ + kcp_alive_timeout;
    auto heartbeat_point = last_write_ + kcp_heartbeat_timeout;
    while (now < deadline_point) {
        deadline_.expires_at(std::min(heartbeat_point, deadline_point));
        if (auto [ec] = co_await deadline_.async_wait(); ec) {
            co_return;
        }

        if (!enable_) {
            co_return;
        }

        deadline_point = last_read_ + kcp_alive_timeout;
        heartbeat_point = last_write_ + kcp_heartbeat_timeout;

        now = std::chrono::steady_clock::now();
        if (now >= heartbeat_point) {
            server_.write_to(remote_, make_kcp_ctrl(kcp_code::heartbeat, socket_id_));
            last_write_ = now;
            heartbeat_point = last_write_ + kcp_heartbeat_timeout;
        }
    }

    stop(socket_errors::kcp_heartbeat_timeout);
}

asio::awaitable<void> kcp_session_impl::co_kcp_update() {
    using namespace std::chrono;
    auto now = asio_timer::clock_type::now();
    auto cur = static_cast<uint32_t>(duration_cast<milliseconds>(now.time_since_epoch()).count());
    auto next = ikcp_check(kcp_, cur);
    auto next_update = now + milliseconds(next - cur);

    while (enable_) {
        kcp_update_.expires_after(milliseconds(10));
        if (auto [ec] = co_await kcp_update_.async_wait(); ec) {
            co_return;
        }

        now = asio_timer::clock_type::now();
        if (now >= next_update || force_update_) {
            force_update_ = false;
            ikcp_update(kcp_, static_cast<uint32_t>(duration_cast<milliseconds>(now.time_since_epoch()).count()));
            now = asio_timer::clock_type::now();
            cur = static_cast<uint32_t>(duration_cast<milliseconds>(now.time_since_epoch()).count());
            next = ikcp_check(kcp_, cur);
            next_update = now + milliseconds(next - cur);
        }
    }
}

bool kcp_session_impl::hand_read_data(const uint8_t* data, size_t len) {
    if (ikcp_getconv(data) != kcp_->conv) {
        stop(socket_errors::kcp_check_failed);
        return false;
    }

    if (ikcp_input(kcp_, reinterpret_cast<const char*>(data), static_cast<long>(len)) < 0) {
        stop(socket_errors::kcp_protocol_error);
        return false;
    }

    force_update_ = true;

    uint8_t temp[kcp_recv_capacity];  // NOLINT(clang-diagnostic-vla-extension)
    const auto& system = socket_system::instance();
    for (;;) {
        const auto recv_bytes = ikcp_recv(kcp_, reinterpret_cast<char*>(temp), kcp_recv_capacity);
        if (recv_bytes <= 0) {
            break;
        }
        system.hand_read(socket_id_, temp, recv_bytes);
    }
    std::ignore = temp;

    return true;
}

}  // namespace simple
