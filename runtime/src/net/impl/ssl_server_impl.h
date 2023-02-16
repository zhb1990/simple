#pragma once

#include <asio/as_tuple.hpp>
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ssl/context.hpp>
#include <asio/use_awaitable.hpp>

#include "socket_impl.hpp"

namespace simple {

class ssl_server_impl final : public socket_base, public std::enable_shared_from_this<ssl_server_impl> {
  public:
    using asio_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using tcp = asio::ip::tcp;
    using tcp_acceptor = asio_token::as_default_on_t<tcp::acceptor>;

    explicit ssl_server_impl(uint32_t socket_id);

    ~ssl_server_impl() noexcept override = default;

    SIMPLE_NON_COPYABLE(ssl_server_impl)

    void start(const tcp::endpoint& endpoint, bool reuse, const std::string& cert, const std::string& key,
               const std::string& dh, const std::string& password);

    void stop(const std::error_code& ec) override;

  private:
    asio::awaitable<void> co_accept();

    tcp_acceptor acceptor_;
    std::shared_ptr<asio::ssl::context> ctx_;
};

}  // namespace simple
