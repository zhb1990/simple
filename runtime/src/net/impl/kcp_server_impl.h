#pragma once

#include <asio/as_tuple.hpp>
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>
#include <asio/use_awaitable.hpp>
#include <unordered_map>

#include "socket_impl.hpp"

namespace simple {

class kcp_session_impl;

class kcp_server_impl final : public socket_base, public std::enable_shared_from_this<kcp_server_impl> {
  public:
    using asio_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using udp = asio::ip::udp;

    explicit kcp_server_impl(uint32_t socket_id);

    ~kcp_server_impl() noexcept override = default;

    DS_NON_COPYABLE(kcp_server_impl)

    void start(const udp::endpoint& endpoint, bool reuse);

    void stop(const std::error_code& ec) override;

    void erase(uint32_t session);

    auto& socket() { return listen_; }

    void write_to(const udp::endpoint& dest, std::vector<uint8_t> data);

  private:
    asio::awaitable<void> co_read();

    void hand_accept(udp::endpoint remote);

    udp::socket listen_;
    std::unordered_map<uint32_t, kcp_session_impl*> sessions_;
};

}  // namespace simple
