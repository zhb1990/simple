#pragma once
#include <asio/as_tuple.hpp>
#include <asio/awaitable.hpp>
#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <deque>

#include "socket_impl.hpp"

// ReSharper disable once IdentifierTypo
// ReSharper disable once CppInconsistentNaming
struct IKCPCB;

namespace simple {

class kcp_server_impl;

class kcp_session_impl final : public socket_base, public std::enable_shared_from_this<kcp_session_impl> {
  public:
    using asio_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using udp = asio::ip::udp;
    using asio_timer = asio_token::as_default_on_t<asio::steady_timer>;

    explicit kcp_session_impl(uint32_t socket_id, udp::endpoint remote, kcp_server_impl& server);

    ~kcp_session_impl() noexcept override;

    SIMPLE_NON_COPYABLE(kcp_session_impl)

    void start(uint32_t acceptor_id);

    void accept() override;

    void stop(const std::error_code& ec) override;

    void write(const memory_buffer_ptr& ptr) override;

    void no_delay(bool on) override;

    void read(uint8_t* data, size_t len);

  private:
    asio::awaitable<void> co_watchdog();

    asio::awaitable<void> co_kcp_update();

    bool hand_read_data(const uint8_t* data, size_t len);

    udp::endpoint remote_;
    kcp_server_impl& server_;

    IKCPCB* kcp_{nullptr};
    asio_timer kcp_update_;
    bool force_update_{false};
    bool enable_{true};

    asio_timer::time_point last_read_;
    asio_timer::time_point last_write_;
    asio_timer deadline_;
};

}  // namespace simple
