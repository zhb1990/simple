#include "tcp_client_impl.h"

#include <simple/log/log.h>
#include <simple/net/socket_system.h>

#include <asio/connect.hpp>
#include <asio/detached.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/write.hpp>

namespace simple {

tcp_client_impl::tcp_client_impl(uint32_t socket_id)  // NOLINT
    : socket_base(socket_id),
      socket_(socket_system::instance().context()),
      write_blocker_(socket_.get_executor()),
      connect_(socket_.get_executor()) {}

void tcp_client_impl::start(const std::string& host, const std::string& service, const asio_timer::duration& timeout) {
    using namespace asio::experimental::awaitable_operators;
    info("tcp client {} start", socket_id_);
    write_blocker_.expires_at(asio_timer::clock_type::time_point::max());
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

void tcp_client_impl::stop(const std::error_code& ec) {
    if (!socket_.is_open()) return;

    info("tcp client {} stop", socket_id_);
    std::error_code ignore;
    socket_.shutdown(tcp::socket::shutdown_both, ignore);
    socket_.close(ignore);
    try {
        write_blocker_.cancel();
        connect_.cancel();
    } catch (...) {
    }

    auto& system = socket_system::instance();
    system.hand_stop(socket_id_, ec);
    system.erase(socket_id_);
}

void tcp_client_impl::write(const memory_buffer_ptr& ptr) {
    trace_write_queue(ptr->readable());
    write_deque_.emplace_back(ptr);
    try {
        write_blocker_.cancel();
    } catch (...) {
    }
}

void tcp_client_impl::no_delay(bool on) {
    std::error_code ec;
    socket_.set_option(tcp::no_delay{on}, ec);
}

asio::awaitable<void> tcp_client_impl::co_connect(const std::string& host, const std::string& service) {
    using tcp_resolver = asio_token::as_default_on_t<tcp::resolver>;
    auto& system = socket_system::instance();
    auto& context = system.context();
    tcp_resolver resolver(context);
    auto [ec, results] = co_await resolver.async_resolve(host, service);
    if (ec) {
        stop(ec);
        co_return;
    }

    info("tcp client {} resolve", socket_id_);
    std::tie(ec, std::ignore) = co_await async_connect(socket_, results);
    if (ec) {
        stop(ec);
        co_return;
    }

    auto self = shared_from_this();
    system.hand_start(socket_id_);

    // 发送协程
    co_spawn(
        context,
        [self, this]() {
            std::ignore = self;
            return co_write();
        },
        asio::detached);

    // 接收协程
    co_spawn(
        context,
        [self, this]() {
            std::ignore = self;
            return co_read();
        },
        asio::detached);
}

asio::awaitable<void> tcp_client_impl::co_timeout(const asio_timer::duration& timeout) {
    connect_.expires_after(timeout);
    if (auto [ec] = co_await connect_.async_wait(); !ec) {
        stop(std::make_error_code(std::errc::timed_out));
    }
}

asio::awaitable<void> tcp_client_impl::co_read() {
    const auto& system = socket_system::instance();
    uint8_t data[1024];
    for (;;) {
        auto [ec, len] = co_await socket_.async_read_some(asio::buffer(data));
        if (ec || len == 0) {
            stop(ec);
            co_return;
        }

        system.hand_read(socket_id_, data, len);
        trace_read(len);
    }
}

asio::awaitable<void> tcp_client_impl::co_write() {
    std::vector<memory_buffer_ptr> cache_write;
    std::vector<asio::const_buffer> buffers;
    const auto max_buffers = socket_system::instance().max_buffers();
    cache_write.reserve(max_buffers);
    buffers.reserve(max_buffers);

    while (socket_.is_open()) {
        if (write_deque_.empty()) {
            co_await write_blocker_.async_wait();
            if (!socket_.is_open()) co_return;
        }

        const auto size = std::min(write_deque_.size(), max_buffers);
        const auto it_begin = write_deque_.begin();
        const auto it_end = it_begin + static_cast<int64_t>(size);
        for (auto it = it_begin; it != it_end; ++it) {
            memory_buffer_ptr temp = *it;
            buffers.emplace_back(temp->begin_read(), temp->readable());
            cache_write.emplace_back(std::move(temp));
        }

        write_deque_.erase(it_begin, it_end);
        auto [ec, len] = co_await async_write(socket_, buffers);
        buffers.clear();
        cache_write.clear();
        trace_write_queue(-static_cast<int64_t>(len));
        trace_write(len);
    }
}

}  // namespace simple
