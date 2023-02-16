#include "ssl_session_impl.h"

#include <simple/log/log.h>
#include <simple/net/socket_system.h>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

namespace simple {

ssl_session_impl::ssl_session_impl(uint32_t socket_id, tcp::socket socket, std::shared_ptr<asio::ssl::context> ctx)  // NOLINT
    : socket_base(socket_id), ctx_(std::move(ctx)), socket_(std::move(socket), *ctx_), write_blocker_(socket_.get_executor()) {}

constexpr ssl_session_impl::asio_token use_awaitable_as_tuple;

void ssl_session_impl::start(uint32_t acceptor_id) {
    info("ssl session {} acceptor:{} start", socket_id_, acceptor_id);
    auto self = shared_from_this();
    auto& system = socket_system::instance();
    co_spawn(
        system.context(),
        [self, acceptor_id, this]() -> asio::awaitable<void> {
            std::ignore = self;
            co_await co_handshake(acceptor_id);
        },
        asio::detached);
}

void ssl_session_impl::accept() {
    auto self = shared_from_this();
    auto& system = socket_system::instance();
    auto& context = system.context();
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

void ssl_session_impl::stop(const std::error_code& ec) {
    auto& socket_raw = socket_.next_layer();
    if (!socket_raw.is_open()) return;

    info("ssl session {} stop", socket_id_);
    std::error_code ignore;
    socket_.shutdown(ignore);
    socket_raw.close(ignore);
    try {
        write_blocker_.cancel();
    } catch (...) {
    }
    auto& system = socket_system::instance();
    system.hand_stop(socket_id_, ec);
    system.erase(socket_id_);
}

void ssl_session_impl::write(const memory_buffer_ptr& ptr) {
    trace_write_queue(ptr->readable());
    write_deque_.emplace_back(ptr);
    try {
        write_blocker_.cancel();
    } catch (...) {
    }
}

void ssl_session_impl::no_delay(bool on) {
    std::error_code ec;
    socket_.next_layer().set_option(tcp::no_delay{on}, ec);
}

asio::awaitable<void> ssl_session_impl::co_handshake(uint32_t acceptor_id) {
    // ssl握手
    if (auto [ec] = co_await socket_.async_handshake(ssl_socket::server, use_awaitable_as_tuple); ec) {
        stop(ec);
        co_return;
    }

    write_blocker_.expires_at(asio_timer::clock_type::time_point::max());
    auto& system = socket_system::instance();
    auto self = shared_from_this();
    system.insert(socket_id_, self);

    const auto& socket_raw = socket_.next_layer();
    std::error_code ignore;
    auto local = socket_raw.local_endpoint(ignore);
    auto remote = socket_raw.remote_endpoint(ignore);
    system.hand_accept(acceptor_id, socket_id_, to_string(local), to_string(remote));
}

asio::awaitable<void> ssl_session_impl::co_read() {
    const auto& system = socket_system::instance();
    uint8_t data[1024];
    for (;;) {
        auto [ec, len] = co_await socket_.async_read_some(asio::buffer(data), use_awaitable_as_tuple);
        if (ec || len == 0) {
            stop(ec);
            co_return;
        }

        system.hand_read(socket_id_, data, len);
        trace_read(len);
    }
}

asio::awaitable<void> ssl_session_impl::co_write() {
    std::vector<memory_buffer_ptr> cache_write;
    std::vector<asio::const_buffer> buffers;
    const auto max_buffers = socket_system::instance().max_buffers();
    cache_write.reserve(max_buffers);
    buffers.reserve(max_buffers);
    const auto& socket_raw = socket_.next_layer();

    while (socket_raw.is_open()) {
        if (write_deque_.empty()) {
            co_await write_blocker_.async_wait();
            if (!socket_raw.is_open()) co_return;
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
        auto [ec, len] = co_await async_write(socket_, buffers, use_awaitable_as_tuple);
        buffers.clear();
        cache_write.clear();
        trace_write_queue(-static_cast<int64_t>(len));
        trace_write(len);
    }
}

}  // namespace simple
