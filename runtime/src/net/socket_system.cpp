#include <simple/error.h>
#include <simple/log/log.h>
#include <simple/net/socket_system.h>
#include <simple/utils/os.h>

#include <asio/buffer.hpp>
#include <asio/detail/buffer_sequence_adapter.hpp>
#include <asio/ip/udp.hpp>
#include <vector>

#include "impl/kcp_client_impl.h"
#include "impl/kcp_server_impl.h"
#include "impl/ssl_client_impl.h"
#include "impl/ssl_server_impl.h"
#include "impl/tcp_client_impl.h"
#include "impl/tcp_server_impl.h"

namespace simple {

socket_system::socket_system() : signals_(context_) {
    // asio定义的不是public，探测下asio 最大发送多少个数据包
    using buffer_sequence = std::vector<asio::const_buffer>;
    buffer_sequence sequence;
    // win iocp 最大 64
    constexpr std::size_t max_send = 64;
    sequence.resize(max_send);
    using buffer_sequence_adapter = asio::detail::buffer_sequence_adapter<asio::const_buffer, buffer_sequence>;
    const buffer_sequence_adapter adapter(sequence);
    max_buffers_ = adapter.count();

    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif

    signals_.async_wait([&](const std::error_code&, int signal_number) {
        context_.stop();
        // 其他模块信号的处理
        {
            std::scoped_lock lock(mutex_single_);
            for (const auto& cb : single_) {
                cb(signal_number);
            }
        }
    });
}

socket_system::~socket_system() noexcept {
    if (!context_.stopped()) {
        context_.stop();
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

socket_system& socket_system::instance() {
    static socket_system system;
    return system;
}

void socket_system::start() {
    thread_ = std::thread{[this]() {
        try {
            context_.run();
        } catch (std::exception& e) {
            critical("socket thread {}", e.what());
        }
    }};
}

void socket_system::stop() {
    if (!context_.stopped()) {
        context_.stop();
    }
}

void socket_system::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

uint32_t socket_system::new_socket_id(socket_type tp) {
    const auto temp = static_cast<uint32_t>(tp);
    if (temp > socket_type_mask) {
        return 0;
    }

    auto& atomic_id = socket_ids_[temp];

    auto id = atomic_id.fetch_add(1, std::memory_order::relaxed) << 4 | temp;

    do {
        {
            std::scoped_lock lock(mutex_sockets_);
            if (!sockets_.contains(id)) {
                break;
            }
        }

        id = atomic_id.fetch_add(1, std::memory_order::relaxed) << 4 | temp;
    } while (true);

    return id;
}

uint32_t socket_system::tcp_listen(const std::string& host, uint16_t port, bool reuse) {
    using namespace asio::ip;
    tcp::endpoint local;
    if (host.empty()) {
        // ReSharper disable once CppAssignedValueIsNeverUsed
        local = {tcp::v6(), port};
    } else {
        std::error_code ec;
        auto address = make_address(host.c_str(), ec);
        if (ec) {
            warn("tcp listen {} make address fail", host);
            return 0;
        }
        // ReSharper disable once CppAssignedValueIsNeverUsed
        local = {address, port};
    }

    const auto socket_id = new_socket_id(socket_type::tcp_server);
    if (socket_id == 0) {
        warn("tcp listen {},{} fail, no new socket id", host, port);
        return 0;
    }

    auto server = std::make_shared<tcp_server_impl>(socket_id);
    post(context_, [server, address = std::move(local), reuse]() { return server->start(address, reuse); });
    return socket_id;
}

uint32_t socket_system::ssl_listen(const std::string& host, uint16_t port, bool reuse, const std::string& cert,
                                   const std::string& key, const std::string& dh, const std::string& password) {
    using namespace asio::ip;
    tcp::endpoint local;
    if (host.empty()) {
        // ReSharper disable once CppAssignedValueIsNeverUsed
        local = {tcp::v6(), port};
    } else {
        std::error_code ec;
        auto address = make_address(host.c_str(), ec);
        if (ec) {
            warn("ssl listen {} make address fail", host);
            return 0;
        }
        // ReSharper disable once CppAssignedValueIsNeverUsed
        local = {address, port};
    }

    const auto socket_id = new_socket_id(socket_type::ssl_server);
    if (socket_id == 0) {
        warn("ssl listen {},{} fail, no new socket id", host, port);
        return 0;
    }

    auto server = std::make_shared<ssl_server_impl>(socket_id);
    post(context_, [server, address = std::move(local), reuse, cert, key, dh, password]() {
        return server->start(address, reuse, cert, key, dh, password);
    });
    return socket_id;
}

uint32_t socket_system::kcp_listen(const std::string& host, uint16_t port, bool reuse) {
    using namespace asio::ip;
    udp::endpoint local;
    if (host.empty()) {
        // ReSharper disable once CppAssignedValueIsNeverUsed
        local = {udp::v6(), port};
    } else {
        std::error_code ec;
        auto address = make_address(host.c_str(), ec);
        if (ec) {
            warn("kcp listen {} make address fail", host);
            return 0;
        }
        // ReSharper disable once CppAssignedValueIsNeverUsed
        local = {address, port};
    }

    const auto socket_id = new_socket_id(socket_type::kcp_server);
    if (socket_id == 0) {
        warn("kcp listen {},{} fail, no new socket id", host, port);
        return 0;
    }

    auto server = std::make_shared<kcp_server_impl>(socket_id);
    post(context_, [server, address = std::move(local), reuse]() { return server->start(address, reuse); });
    return socket_id;
}

uint32_t socket_system::tcp_connect(const std::string& host, const std::string& service,
                                    const std::chrono::milliseconds& timeout) {
    const auto socket_id = new_socket_id(socket_type::tcp_client);
    if (socket_id == 0) {
        warn("tcp connect {},{} fail, no new socket id", host, service);
        return 0;
    }

    auto client = std::make_shared<tcp_client_impl>(socket_id);
    post(context_, [client, host, service, timeout]() { return client->start(host, service, timeout); });
    return socket_id;
}

uint32_t socket_system::ssl_connect(const std::string& host, const std::string& service,
                                    const std::chrono::milliseconds& timeout, const std::string& verify, bool ignore_cert) {
    const auto socket_id = new_socket_id(socket_type::ssl_client);
    if (socket_id == 0) {
        warn("ssl connect {},{} fail, no new socket id", host, service);
        return 0;
    }

    auto client = std::make_shared<ssl_client_impl>(socket_id);
    post(context_, [client, host, service, timeout, verify, ignore_cert]() {
        return client->start(host, service, timeout, verify, ignore_cert);
    });
    return socket_id;
}

uint32_t socket_system::kcp_connect(const std::string& host, const std::string& service,
                                    const std::chrono::milliseconds& timeout) {
    const auto socket_id = new_socket_id(socket_type::kcp_client);
    if (socket_id == 0) {
        warn("kcp connect {},{} fail, no new socket id", host, service);
        return 0;
    }

    auto client = std::make_shared<kcp_client_impl>(socket_id);
    post(context_, [client, host, service, timeout]() { return client->start(host, service, timeout); });
    return socket_id;
}

void socket_system::send(uint32_t socket_id, const memory_buffer_ptr& buf) {
    if (get_socket_class(socket_id) == socket_class::server) {
        return;
    }

    post(context_, [socket_id, buf, this]() {
        if (const auto ptr = find(socket_id)) {
            ptr->write(buf);
        }
    });
}

void socket_system::accept(uint32_t socket_id) {
    if (get_socket_class(socket_id) != socket_class::session) {
        return;
    }

    post(context_, [socket_id, this]() {
        if (const auto ptr = find(socket_id)) {
            ptr->accept();
        }
    });
}

void socket_system::close(uint32_t socket_id) {
    post(context_, [socket_id, this]() {
        if (const auto ptr = find(socket_id)) {
            ptr->stop(socket_errors::initiative_disconnect);
        }
    });
}

void socket_system::no_delay(uint32_t socket_id, bool on) {
    if (get_socket_class(socket_id) == socket_class::server) {
        return;
    }

    post(context_, [socket_id, on, this]() {
        if (const auto ptr = find(socket_id)) {
            ptr->no_delay(on);
        }
    });
}

void socket_system::hand_start(uint32_t socket_id) const { start_(socket_id); }

void socket_system::hand_stop(uint32_t socket_id, const std::error_code& ec) const { stop_(socket_id, ec); }

void socket_system::hand_read(uint32_t socket_id, const uint8_t* data, size_t len) const { read_(socket_id, data, len); }

void socket_system::hand_accept(uint32_t socket_id, uint32_t accepted, const std::string& local,
                                const std::string& remote) const {
    accept_(socket_id, accepted, local, remote);
}

void socket_system::insert(uint32_t socket_id, const socket_base_ptr& ptr) {
    std::scoped_lock lock(mutex_sockets_);
    sockets_.emplace(socket_id, ptr);
}

void socket_system::erase(uint32_t socket_id) {
    std::scoped_lock lock(mutex_sockets_);
    sockets_.erase(socket_id);
}

socket_system::socket_base_ptr socket_system::find(uint32_t socket_id) {
    std::scoped_lock lock(mutex_sockets_);
    if (const auto it = sockets_.find(socket_id); it != sockets_.end()) {
        return it->second;
    }

    return {};
}

}  // namespace simple
