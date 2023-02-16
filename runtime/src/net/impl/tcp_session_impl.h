﻿#pragma once

#include <asio/as_tuple.hpp>
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <deque>

#include "socket_impl.hpp"

namespace simple {

class tcp_session_impl final : public socket_base, public std::enable_shared_from_this<tcp_session_impl> {
  public:
    using asio_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using tcp = asio::ip::tcp;
    using tcp_socket = asio_token::as_default_on_t<tcp::socket>;
    using asio_timer = asio_token::as_default_on_t<asio::steady_timer>;

    tcp_session_impl(uint32_t socket_id, tcp_socket socket);

    ~tcp_session_impl() noexcept override = default;

    DS_NON_COPYABLE(tcp_session_impl)

    void start(uint32_t acceptor_id);

    void accept() override;

    void stop(const std::error_code& ec) override;

    void write(const memory_buffer_ptr& ptr) override;

    void no_delay(bool on) override;

  private:
    asio::awaitable<void> co_read();

    asio::awaitable<void> co_write();

    tcp_socket socket_;
    asio_timer write_blocker_;
    std::deque<memory_buffer_ptr> write_deque_;
};

}  // namespace simple
