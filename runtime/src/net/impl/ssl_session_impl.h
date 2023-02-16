#pragma once

#include <asio/as_tuple.hpp>
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ssl/context.hpp>
#include <asio/ssl/stream.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <deque>

#include "socket_impl.hpp"

namespace simple {

class ssl_session_impl final : public socket_base, public std::enable_shared_from_this<ssl_session_impl> {
  public:
    using asio_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using tcp = asio::ip::tcp;
    using ssl_socket = asio::ssl::stream<tcp::socket>;
    using asio_timer = asio_token::as_default_on_t<asio::steady_timer>;

    ssl_session_impl(uint32_t socket_id, tcp::socket socket, std::shared_ptr<asio::ssl::context> ctx);

    ~ssl_session_impl() noexcept override = default;

    DS_NON_COPYABLE(ssl_session_impl)

    void start(uint32_t acceptor_id);

    void accept() override;

    void stop(const std::error_code& ec) override;

    void write(const memory_buffer_ptr& ptr) override;

    void no_delay(bool on) override;

  private:
    asio::awaitable<void> co_handshake(uint32_t acceptor_id);

    asio::awaitable<void> co_read();

    asio::awaitable<void> co_write();

    std::shared_ptr<asio::ssl::context> ctx_;
    ssl_socket socket_;
    asio_timer write_blocker_;
    std::deque<memory_buffer_ptr> write_deque_;
};

}  // namespace simple
