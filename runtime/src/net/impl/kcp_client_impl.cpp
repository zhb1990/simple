#include "kcp_client_impl.h"

#include <simple/error.h>
#include <simple/log/log.h>
#include <simple/net/socket_system.h>

#include <asio/connect.hpp>
#include <asio/detached.hpp>
#include <asio/experimental/awaitable_operators.hpp>

#include "kcp_config.h"

namespace simple {

kcp_client_impl::kcp_client_impl(uint32_t socket_id)  // NOLINT(cppcoreguidelines-pro-type-member-init)
    : socket_base(socket_id),
      socket_(socket_system::instance().context()),
      kcp_update_(socket_.get_executor()),
      deadline_(socket_.get_executor()) {}

kcp_client_impl::~kcp_client_impl() noexcept {
    if (kcp_) {
        ikcp_release(kcp_);
    }
}

void kcp_client_impl::start(const std::string& host, const std::string& service, const asio_timer::duration& timeout) {
    using namespace asio::experimental::awaitable_operators;
    info("kcp client {} start", socket_id_);
    auto self = shared_from_this();
    auto& system = socket_system::instance();
    system.insert(socket_id_, self);

    co_spawn(
        system.context(),
        [self, timeout, host, service, this]() -> asio::awaitable<void> {
            std::ignore = self;
            co_await (co_connect(host, service) || co_timeout(timeout));
        },
        asio::detached);
}

void kcp_client_impl::stop(const std::error_code& ec) {
    if (state_ == state::closed) return;
    info("kcp client {} stop", socket_id_);
    state_ = state::closed;
    std::error_code ignore;
    socket_.close(ignore);

    try {
        kcp_update_.cancel();
        deadline_.cancel();
    } catch (...) {
    }

    auto& system = socket_system::instance();
    system.hand_stop(socket_id_, ec);
    system.erase(socket_id_);
}

void kcp_client_impl::write(const memory_buffer_ptr& ptr) {
    if (state_ != state::connected) {
        write_deque_.emplace_back(ptr);
        return;
    }

    if (write_base(ptr)) {
        force_update_ = true;
    }
}

void kcp_client_impl::no_delay(bool on) {
    if (kcp_) {
        kcp_no_delay(kcp_, on);
    }
}

constexpr kcp_client_impl::asio_token use_awaitable_as_tuple;

asio::awaitable<void> kcp_client_impl::co_connect(const std::string& host, const std::string& service) {
    using udp_resolver = asio_token::as_default_on_t<udp::resolver>;
    auto& system = socket_system::instance();
    auto& context = system.context();

    // 域名解析
    udp_resolver resolver(context);
    auto [ec, results] = co_await resolver.async_resolve(host, service);
    if (ec) {
        stop(ec);
        co_return;
    }

    info("kcp client {} resolve", socket_id_);

    std::tie(ec, std::ignore) = co_await async_connect(socket_, results, use_awaitable_as_tuple);
    if (ec) {
        stop(ec);
        co_return;
    }

    info("kcp client {} udp connect", socket_id_);

    // 发送connect
    auto msg = make_kcp_ctrl(kcp_code::connect, 0);
    std::tie(ec, std::ignore) = co_await socket_.async_send(asio::buffer(msg.data(), msg.size()), use_awaitable_as_tuple);
    if (ec) {
        stop(ec);
        co_return;
    }

    info("kcp client {} kcp connect", socket_id_);

    // 接收connect ack
    uint8_t data[udp_mtu];
    size_t len;
    std::tie(ec, len) = co_await socket_.async_receive(asio::buffer(data), use_awaitable_as_tuple);
    if (ec) {
        stop(ec);
        co_return;
    }

    if (const auto* head = reinterpret_cast<kcp_head*>(data); head->magic1 != kcp_magic1 || head->magic2 != kcp_magic2 ||
                                                              head->magic3 != kcp_magic3 ||
                                                              head->code != kcp_code::connect_ack) {
        disconnect(socket_errors::kcp_check_failed);
        co_return;
    }

    if (len < kcp_head_size + sizeof(uint32_t)) {
        stop(socket_errors::kcp_check_failed);
        co_return;
    }

    info("kcp client {} kcp connect ack", socket_id_);

    uint32_t conv;
    memcpy(&conv, data + kcp_head_size, sizeof(conv));
    conv = ntohl(conv);
    kcp_ = kcp_create_default(conv, this);
    state_ = state::connected;
    ikcp_setoutput(kcp_, [](const char* buf, int len, ikcpcb* kcp, void* user) {
        auto* client = static_cast<kcp_client_impl*>(user);
        client->last_write_ = asio_timer::clock_type::now();
        client->write_to(make_kcp_data(buf, len));
        return 0;
    });

    auto self = shared_from_this();
    std::error_code ec_ignore;
    auto local = socket_.local_endpoint(ec_ignore);
    system.hand_start(socket_id_, to_string(local));

    if (!write_deque_.empty()) {
        for (const auto& ptr : write_deque_) {
            if (!write_base(ptr)) {
                co_return;
            }
        }
        write_deque_.clear();
        force_update_ = true;
    }

    last_read_ = asio_timer::clock_type::now();
    last_write_ = last_read_;

    // 接收协程
    co_spawn(
        context,
        [self, this]() {
            std::ignore = self;
            return co_read();
        },
        asio::detached);

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

asio::awaitable<void> kcp_client_impl::co_timeout(const asio_timer::duration& timeout) {
    deadline_.expires_after(timeout);
    if (auto [ec] = co_await deadline_.async_wait(); !ec) {
        stop(std::make_error_code(std::errc::timed_out));
    }
}

asio::awaitable<void> kcp_client_impl::co_read() {
    auto self = shared_from_this();
    uint8_t data[udp_mtu];
    for (;;) {
        auto [ec, len] = co_await socket_.async_receive(asio::buffer(data), use_awaitable_as_tuple);
        if (state_ != state::connected) {
            co_return;
        }

        if (ec || len < kcp_head_size) {
            disconnect(ec);
            co_return;
        }

        const auto* head = reinterpret_cast<kcp_head*>(data);
        if (head->magic1 != kcp_magic1 || head->magic2 != kcp_magic2 || head->magic3 != kcp_magic3) {
            disconnect(socket_errors::kcp_check_failed);
            co_return;
        }

        trace_read(len);
        switch (head->code) {  // NOLINT(clang-diagnostic-switch-enum)
            case kcp_code::disconnect: {
                if (len < kcp_head_size + sizeof(uint32_t)) {
                    continue;
                }

                uint32_t conv = 0;
                memcpy(&conv, data + kcp_head_size, sizeof(conv));
                conv = ntohl(conv);
                if (conv != kcp_->conv) {
                    continue;
                }

                disconnect(asio::error::eof);
                co_return;
            }
            case kcp_code::heartbeat:
                last_write_ = asio_timer::clock_type::now();
                write_to(make_kcp_ctrl(kcp_code::heartbeat_ack, kcp_->conv));
                break;
            case kcp_code::heartbeat_ack:
                break;
            case kcp_code::data:
                if (!hand_read_data(data + kcp_head_size, len - kcp_head_size)) {
                    co_return;
                }
                break;
            default:
                continue;
        }

        last_read_ = asio_timer::clock_type::now();
    }
}

asio::awaitable<void> kcp_client_impl::co_watchdog() {
    auto self = shared_from_this();
    auto now = std::chrono::steady_clock::now();
    auto deadline_point = last_read_ + kcp_alive_timeout;
    auto heartbeat_point = last_write_ + kcp_heartbeat_timeout;
    while (now < deadline_point) {
        deadline_.expires_at(std::min(heartbeat_point, deadline_point));
        if (auto [ec] = co_await deadline_.async_wait(); ec) {
            co_return;
        }

        if (state_ != state::connected) {
            co_return;
        }

        deadline_point = last_read_ + kcp_alive_timeout;
        heartbeat_point = last_write_ + kcp_heartbeat_timeout;

        now = std::chrono::steady_clock::now();
        if (now >= heartbeat_point) {
            write_to(make_kcp_ctrl(kcp_code::heartbeat, kcp_->conv));
            last_write_ = now;
            heartbeat_point = last_write_ + kcp_heartbeat_timeout;
        }
    }

    disconnect(socket_errors::kcp_heartbeat_timeout);
}

asio::awaitable<void> kcp_client_impl::co_kcp_update() {
    using namespace std::chrono;
    auto now = asio_timer::clock_type::now();
    auto cur = static_cast<uint32_t>(duration_cast<milliseconds>(now.time_since_epoch()).count());
    auto next = ikcp_check(kcp_, cur);
    auto next_update = now + milliseconds(next - cur);

    while (state_ == state::connected) {
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

bool kcp_client_impl::write_base(const memory_buffer_ptr& ptr) {
    auto* data = reinterpret_cast<const char*>(ptr->begin_read());
    auto len = static_cast<int>(ptr->readable());

    while (len > kcp_recv_capacity) {
        if (ikcp_send(kcp_, data, kcp_recv_capacity) < 0) {
            disconnect(socket_errors::kcp_protocol_error);
            return false;
        }
        len -= kcp_recv_capacity;
        data += kcp_recv_capacity;
    }

    if (len > 0 && ikcp_send(kcp_, data, len) < 0) {
        disconnect(socket_errors::kcp_protocol_error);
        return false;
    }

    return true;
}

void kcp_client_impl::write_to(std::vector<uint8_t> data) {
    auto self = shared_from_this();
    const auto msg = std::make_shared<std::vector<uint8_t>>(std::move(data));
    socket_.async_send(asio::buffer(msg->data(), msg->size()), [self, msg](const std::error_code&, size_t) {
        std::ignore = self;
        std::ignore = msg;
    });
}

bool kcp_client_impl::hand_read_data(const uint8_t* data, size_t len) {
    if (ikcp_getconv(data) != kcp_->conv) {
        disconnect(socket_errors::kcp_check_failed);
        return false;
    }

    if (ikcp_input(kcp_, reinterpret_cast<const char*>(data), static_cast<long>(len)) < 0) {
        disconnect(socket_errors::kcp_protocol_error);
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

void kcp_client_impl::disconnect(const std::error_code& ec) {
    if (state_ != state::connected) return;
    state_ = state::close_wait;

    auto self = shared_from_this();
    co_spawn(
        socket_.get_executor(),
        [self, ec, this]() -> asio::awaitable<void> {
            auto msg = make_kcp_ctrl(kcp_code::disconnect, kcp_->conv);
            co_await socket_.async_send(asio::buffer(msg.data(), msg.size()), use_awaitable_as_tuple);
            stop(ec);
        },
        asio::detached);
}

}  // namespace simple
