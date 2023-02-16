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

class kcp_client_impl final : public socket_base, public std::enable_shared_from_this<kcp_client_impl> {
  public:
    using asio_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using udp = asio::ip::udp;
    using asio_timer = asio_token::as_default_on_t<asio::steady_timer>;

    explicit kcp_client_impl(uint32_t socket_id);

    ~kcp_client_impl() noexcept override;

    SIMPLE_NON_COPYABLE(kcp_client_impl)

    void start(const std::string& host, const std::string& service, const asio_timer::duration& timeout);

    void stop(const std::error_code& ec) override;

    void write(const memory_buffer_ptr& ptr) override;

    void no_delay(bool on) override;

  private:
    asio::awaitable<void> co_connect(const std::string& host, const std::string& service);

    asio::awaitable<void> co_timeout(const asio_timer::duration& timeout);

    asio::awaitable<void> co_read();

    asio::awaitable<void> co_watchdog();

    asio::awaitable<void> co_kcp_update();

    bool write_base(const memory_buffer_ptr& ptr);

    void write_to(std::vector<uint8_t> data);

    bool hand_read_data(const uint8_t* data, size_t len);

    void disconnect(const std::error_code& ec);

    udp::socket socket_;

    IKCPCB* kcp_{nullptr};
    asio_timer kcp_update_;
    bool force_update_{false};

    asio_timer::time_point last_read_;
    asio_timer::time_point last_write_;
    asio_timer deadline_;
    // 这个和其他的client不同，仅仅是连接未建立成功时缓存消息
    std::deque<memory_buffer_ptr> write_deque_;
    enum class state { normal, connected, close_wait, closed };
    state state_{state::normal};
};

}  // namespace simple
