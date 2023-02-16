#pragma once

#include <asio/as_tuple.hpp>
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>

#include "socket_impl.hpp"

namespace simple {

class tcp_server_impl final : public socket_base, public std::enable_shared_from_this<tcp_server_impl> {
  public:
    using asio_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using tcp = asio::ip::tcp;
    using tcp_acceptor = asio_token::as_default_on_t<tcp::acceptor>;

    explicit tcp_server_impl(uint32_t socket_id);

    ~tcp_server_impl() noexcept override = default;

    DS_NON_COPYABLE(tcp_server_impl)

    void start(const tcp::endpoint& endpoint, bool reuse);

    void stop(const std::error_code& ec) override;

  private:
    asio::awaitable<void> co_accept();

    tcp_acceptor acceptor_;
};

}  // namespace simple
