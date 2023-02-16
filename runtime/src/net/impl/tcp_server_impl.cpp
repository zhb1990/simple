#include "tcp_server_impl.h"

#include <simple/log/log.h>
#include <simple/net/socket_system.h>
#include <simple/utils/os.h>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include "tcp_session_impl.h"

namespace simple {

tcp_server_impl::tcp_server_impl(uint32_t socket_id)  // NOLINT(cppcoreguidelines-pro-type-member-init)
    : socket_base(socket_id), acceptor_(socket_system::instance().context()) {}

void tcp_server_impl::start(const tcp::endpoint& endpoint, bool reuse) {
    info("tcp server {} start", socket_id_);
    std::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) return stop(ec);
    acceptor_.set_option(tcp::acceptor::reuse_address(reuse), ec);
    if (ec) return stop(ec);
    acceptor_.bind(endpoint, ec);
    if (ec) return stop(ec);
    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) return stop(ec);

    auto self = shared_from_this();
    auto& system = socket_system::instance();
    system.insert(socket_id_, self);
    system.hand_start(socket_id_);

    co_spawn(
        system.context(),
        [self, this]() {
            std::ignore = self;
            return co_accept();
        },
        asio::detached);
}

void tcp_server_impl::stop(const std::error_code& ec) {
    if (!acceptor_.is_open()) return;

    info("tcp server {} stop", socket_id_);
    std::error_code ignore;
    acceptor_.close(ignore);
    auto& system = socket_system::instance();
    system.hand_stop(socket_id_, ec);
    system.erase(socket_id_);
}

asio::awaitable<void> tcp_server_impl::co_accept() {
    auto& system = socket_system::instance();
    while (acceptor_.is_open()) {
        if (auto [ec, socket] = co_await acceptor_.async_accept(); socket.is_open()) {
            const auto id = system.new_socket_id(socket_type::tcp_session);
            if (id == 0) {
                warn("tcp server {} accept fail, no new socket id", socket_id_);
                std::error_code ignore;
                socket.close(ignore);
                continue;
            }
            trace_read(1);
            const auto session = std::make_shared<tcp_session_impl>(id, std::move(socket));
            session->start(socket_id_);
        } else {
            warn("tcp server {} accept fail, {}", socket_id_, ERROR_CODE_MESSAGE(ec.message()));
        }
    }
}

}  // namespace simple
