#include "tcp_session_impl.h"

#include <simple/log/log.h>
#include <simple/net/socket_system.h>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/write.hpp>

namespace simple {

tcp_session_impl::tcp_session_impl(uint32_t socket_id, tcp_socket socket)  // NOLINT
    : socket_base(socket_id), socket_(std::move(socket)), write_blocker_(socket_.get_executor()) {}

void tcp_session_impl::start(uint32_t acceptor_id) {
    info("tcp session {} acceptor:{} start", socket_id_, acceptor_id);
    write_blocker_.expires_at(asio_timer::clock_type::time_point::max());
    auto& system = socket_system::instance();
    const auto self = shared_from_this();
    system.insert(socket_id_, self);

    std::error_code ignore;
    auto local = socket_.local_endpoint(ignore);
    auto remote = socket_.remote_endpoint(ignore);
    system.hand_accept(acceptor_id, socket_id_, to_string(local), to_string(remote));
}

void tcp_session_impl::accept() {
    auto& system = socket_system::instance();
    auto& context = system.context();
    auto self = shared_from_this();
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

void tcp_session_impl::stop(const std::error_code& ec) {
    if (!socket_.is_open()) return;

    info("tcp session {} stop", socket_id_);
    std::error_code ignore;
    socket_.shutdown(tcp::socket::shutdown_both, ignore);
    socket_.close(ignore);
    try {
        write_blocker_.cancel();
    } catch (...) {
    }
    write_blocker_.cancel();
    auto& system = socket_system::instance();
    system.hand_stop(socket_id_, ec);
    system.erase(socket_id_);
}

void tcp_session_impl::write(const memory_buffer_ptr& ptr) {
    trace_write_queue(ptr->readable());
    write_deque_.emplace_back(ptr);
    try {
        write_blocker_.cancel();
    } catch (...) {
    }
}

void tcp_session_impl::no_delay(bool on) {
    std::error_code ec;
    socket_.set_option(tcp::no_delay{on}, ec);
}

asio::awaitable<void> tcp_session_impl::co_read() {
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

asio::awaitable<void> tcp_session_impl::co_write() {
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
