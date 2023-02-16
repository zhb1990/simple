#include <fmt/format.h>
#include <simple/coro/cancellation_registration.h>
#include <simple/coro/cancellation_token.h>
#include <simple/coro/network.h>
#include <simple/coro/scheduler.h>
#include <simple/error.h>
#include <simple/net/socket_system.h>

#include <coroutine>
#include <deque>
#include <optional>
#include <simple/containers/buffer.hpp>

namespace simple {

struct network_data {
    uint32_t id{0};
    std::error_code ec;
    memory_buffer buf;
    std::deque<uint32_t> accepted;
    std::coroutine_handle<> handle;
    std::string local;
    std::string remote;
};

class network_awaiter {
  public:
    explicit network_awaiter(network_data_ptr s) : socket_(std::move(s)) {}

    network_awaiter(const network_awaiter&) = delete;

    network_awaiter(network_awaiter&& other) noexcept : socket_(std::move(other.socket_)) {}

    ~network_awaiter() noexcept = default;

    network_awaiter& operator=(network_awaiter&) = delete;

    network_awaiter& operator=(network_awaiter&&) noexcept = delete;

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return !socket_; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        handle_ = handle;
        token_ = handle.promise().get_cancellation_token();

        if (socket_->handle || token_.is_cancellation_requested()) {
            return false;
        }

        if (token_.can_be_cancelled()) {
            registration_.emplace(token_, [this]() {
                network::instance().remove_socket(socket_->id);
                scheduler::instance().wake_up_coroutine(handle_);
            });
        }

        socket_->handle = handle_;
        return true;
    }

    void await_resume() {
        static const std::error_code eof = asio::error::eof;

        registration_.reset();
        if (token_.is_cancellation_requested()) {
            throw std::system_error(coro_errors::canceled);
        }

        if (!socket_) return;

        if (socket_->handle != handle_) {
            throw std::system_error(coro_errors::invalid_action);
        }

        if (socket_->ec && socket_->ec != eof) {
            throw std::system_error(socket_->ec);
        }

        socket_->handle = nullptr;
    }

  private:
    network_data_ptr socket_;
    std::coroutine_handle<> handle_;
    cancellation_token token_;
    std::optional<cancellation_registration> registration_;
};

network& network::instance() {
    static network ins;
    return ins;
}

template <typename Service>
std::string to_address_string(const std::string& host, const Service& service) {
    try {
        if (host.find(':') == std::string::npos) {
            return fmt::format("[{}]:{}", host, service);
        }

        return fmt::format("{}:{}", host, service);
    } catch (...) {
        return {};
    }
}

template <typename Service>
network_awaiter network::create_start_awaiter(uint32_t socket_id, const std::string& host, const Service& service) {
    auto ptr = std::make_shared<network_data>();
    ptr->id = socket_id;
    ptr->local = to_address_string(host, service);
    sockets_.emplace(socket_id, ptr);

    return network_awaiter(std::move(ptr));
}

task<uint32_t> network::tcp_listen(const std::string& host, uint16_t port, bool reuse) {
    const auto id = socket_system::instance().tcp_listen(host, port, reuse);
    co_await create_start_awaiter(id, host, port);
    co_return id;
}

task<uint32_t> network::ssl_listen(const std::string& host, uint16_t port, bool reuse, const std::string& cert,
                                   const std::string& key, const std::string& dh, const std::string& password) {
    const auto id = socket_system::instance().ssl_listen(host, port, reuse, cert, key, dh, password);
    co_await create_start_awaiter(id, host, port);
    co_return id;
}

task<uint32_t> network::kcp_listen(const std::string& host, uint16_t port, bool reuse) {
    const auto id = socket_system::instance().kcp_listen(host, port, reuse);
    co_await create_start_awaiter(id, host, port);
    co_return id;
}

task<uint32_t> network::tcp_connect(const std::string& host, const std::string& service,
                                    const std::chrono::milliseconds& timeout) {
    const auto id = socket_system::instance().tcp_connect(host, service, timeout);
    co_await create_start_awaiter(id, host, service);
    co_return id;
}

task<uint32_t> network::ssl_connect(const std::string& host, const std::string& service,
                                    const std::chrono::milliseconds& timeout, const std::string& verify, bool ignore_cert) {
    const auto id = socket_system::instance().ssl_connect(host, service, timeout, verify, ignore_cert);
    co_await create_start_awaiter(id, host, service);
    co_return id;
}

task<uint32_t> network::kcp_connect(const std::string& host, const std::string& service,
                                    const std::chrono::milliseconds& timeout) {
    const auto id = socket_system::instance().kcp_connect(host, service, timeout);
    co_await create_start_awaiter(id, host, service);
    co_return id;
}

task<uint32_t> network::accept(uint32_t listen_id) {
    if (get_socket_class(listen_id) != socket_class::server) {
        throw std::system_error(coro_errors::invalid_action);
    }

    const auto it = sockets_.find(listen_id);
    if (it == sockets_.end()) {
        throw std::system_error(coro_errors::invalid_action);
    }

    const auto ptr = it->second;
    if (ptr->accepted.empty()) {
        co_await network_awaiter(ptr);
        if (ptr->accepted.empty()) {
            // 不可能进这里
            co_return 0;
        }
    }

    auto accepted = ptr->accepted.front();
    ptr->accepted.pop_front();

    socket_system::instance().accept(accepted);

    co_return accepted;
}

task<size_t> network::read(uint32_t socket_id, void* buf, size_t size) {
    if (get_socket_class(socket_id) == socket_class::server) {
        throw std::system_error(coro_errors::invalid_action);
    }

    const auto it = sockets_.find(socket_id);
    if (it == sockets_.end()) {
        throw std::system_error(coro_errors::invalid_action);
    }

    const auto ptr = it->second;
    if (ptr->buf.readable() == 0) {
        co_await network_awaiter(ptr);
        if (ptr->ec) {
            co_return 0;
        }
    }

    auto len = std::min(ptr->buf.readable(), size);
    memcpy(buf, ptr->buf.begin_read(), len);
    ptr->buf.read(len);
    co_return len;
}

task<size_t> network::read_size(uint32_t socket_id, void* buf, size_t size) {
    if (get_socket_class(socket_id) == socket_class::server) {
        throw std::system_error(coro_errors::invalid_action);
    }

    const auto it = sockets_.find(socket_id);
    if (it == sockets_.end()) {
        throw std::system_error(coro_errors::invalid_action);
    }

    const auto ptr = it->second;
    auto readable = ptr->buf.readable();
    while (readable < size) {
        co_await network_awaiter(ptr);
        if (ptr->ec) {
            co_return 0;
        }
        readable = ptr->buf.readable();
    }

    memcpy(buf, ptr->buf.begin_read(), size);
    ptr->buf.read(size);
    co_return size;
}

task<memory_buffer_ptr> network::read_until(uint32_t socket_id, std::string_view end) {
    auto result = std::make_shared<memory_buffer>();
    if (co_await read_until(socket_id, end, *result) == 0) {
        co_return memory_buffer_ptr{};
    }

    co_return result;
}

task<size_t> network::read_until(uint32_t socket_id, std::string_view end, memory_buffer& buf) {
    if (end.empty()) {
        throw std::system_error(coro_errors::invalid_action);
    }

    if (get_socket_class(socket_id) == socket_class::server) {
        throw std::system_error(coro_errors::invalid_action);
    }

    const auto it = sockets_.find(socket_id);
    if (it == sockets_.end()) {
        throw std::system_error(coro_errors::invalid_action);
    }

    const auto ptr = it->second;
    const auto end_size = end.size();

    for (;;) {
        auto strv = std::string_view(ptr->buf);
        if (const auto pos = strv.find(end); pos != std::string_view::npos) {
            const auto len = pos + end_size;
            buf.append(strv.data(), len);
            ptr->buf.read(len);
            break;
        }

        if (strv.size() >= end_size) {
            const auto len = strv.size() - end_size + 1;
            buf.append(strv.data(), len);
            ptr->buf.read(len);
        }

        co_await network_awaiter(ptr);
        if (ptr->ec) {
            co_return 0;
        }
    }

    co_return buf.readable();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void network::write(uint32_t socket_id, const memory_buffer_ptr& buf) { socket_system::instance().send(socket_id, buf); }

void network::close(uint32_t socket_id) {
    socket_system::instance().close(socket_id);
    hand_stop(socket_id, socket_errors::initiative_disconnect);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void network::no_delay(uint32_t socket_id, bool on) { socket_system::instance().no_delay(socket_id, on); }

std::string network::local_address(uint32_t socket_id) {
    if (const auto it = sockets_.find(socket_id); it != sockets_.end()) {
        return it->second->local;
    }

    return {};
}

std::string network::remote_address(uint32_t socket_id) {
    if (const auto it = sockets_.find(socket_id); it != sockets_.end()) {
        return it->second->remote;
    }

    return {};
}

void network::init() {
    socket_system& system = socket_system::instance();
    auto& scheduler = scheduler::instance();
    system.register_accept_handle(
        [this, &scheduler](uint32_t socket_id, uint32_t accepted, const std::string& local, const std::string& remote) {
            return scheduler.post(
                [this, socket_id, accepted, local, remote] { return hand_accept(socket_id, accepted, local, remote); });
        });

    system.register_start_handle(
        [this, &scheduler](uint32_t socket_id) { return scheduler.post([this, socket_id] { return hand_start(socket_id); }); });

    system.register_stop_handle([this, &scheduler](uint32_t socket_id, const std::error_code& ec) {
        return scheduler.post([this, socket_id, ec] { return hand_stop(socket_id, ec); });
    });

    system.register_read_handle([this, &scheduler](uint32_t socket_id, const uint8_t* data, size_t len) {
        return scheduler.post([this, socket_id, buf = memory_buffer(data, len)] { return hand_read(socket_id, buf); });
    });
}

bool network::remove_socket(uint32_t socket_id) { return sockets_.erase(socket_id) > 0; }

void network::hand_start(uint32_t socket_id) {
    if (const auto it = sockets_.find(socket_id); it != sockets_.end() && it->second->handle) {
        it->second->handle.resume();
    }
}

void network::hand_stop(uint32_t socket_id, const std::error_code& ec) {
    const auto it = sockets_.find(socket_id);
    if (it == sockets_.end()) {
        return;
    }

    const auto ptr = std::move(it->second);
    sockets_.erase(it);
    ptr->ec = ec;
    if (ptr->handle) {
        ptr->handle.resume();
    }
}

void network::hand_read(uint32_t socket_id, const memory_buffer& data) {
    const auto it = sockets_.find(socket_id);
    if (it == sockets_.end()) {
        return;
    }

    if (it->second->buf.prependable() >= it->second->buf.capacity() / 4) {
        // 如果已读的长度超过4分之1则进行收缩
        it->second->buf.shrink();
    }

    it->second->buf.append(data.begin_read(), data.readable());
    if (it->second->handle) {
        it->second->handle.resume();
    }
}

void network::hand_accept(uint32_t socket_id, uint32_t accepted, const std::string& local, const std::string& remote) {
    const auto it = sockets_.find(socket_id);
    if (it == sockets_.end()) {
        close(accepted);
        return;
    }

    auto ptr = std::make_shared<network_data>();
    ptr->id = accepted;
    ptr->local = local;
    ptr->remote = remote;
    sockets_.emplace(accepted, std::move(ptr));

    it->second->accepted.emplace_back(accepted);
    if (it->second->handle) {
        it->second->handle.resume();
    }
}

}  // namespace simple
