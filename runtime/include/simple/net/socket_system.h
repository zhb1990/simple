#pragma once
#include <simple/config.h>
#include <simple/net/socket_types.h>

#include <simple/containers/buffer.hpp>

//
#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace simple {

class socket_base;

class socket_system {
    socket_system();

  public:
    using socket_base_ptr = std::shared_ptr<socket_base>;

    ~socket_system() noexcept;

    DS_NON_COPYABLE(socket_system)

    DS_API static socket_system& instance();

    DS_API void start();

    DS_API void stop();

    DS_API void join();

    uint32_t new_socket_id(socket_type tp);

    DS_API uint32_t tcp_listen(const std::string& host, uint16_t port, bool reuse);

    DS_API uint32_t ssl_listen(const std::string& host, uint16_t port, bool reuse, const std::string& cert,
                               const std::string& key, const std::string& dh, const std::string& password);

    DS_API uint32_t kcp_listen(const std::string& host, uint16_t port, bool reuse);

    DS_API uint32_t tcp_connect(const std::string& host, const std::string& service, const std::chrono::milliseconds& timeout);

    DS_API uint32_t ssl_connect(const std::string& host, const std::string& service, const std::chrono::milliseconds& timeout,
                                const std::string& verify = "", bool ignore_cert = true);

    DS_API uint32_t kcp_connect(const std::string& host, const std::string& service, const std::chrono::milliseconds& timeout);

    DS_API void send(uint32_t socket_id, const memory_buffer_ptr& buf);

    DS_API void accept(uint32_t socket_id);

    DS_API void close(uint32_t socket_id);

    DS_API void no_delay(uint32_t socket_id, bool on);

    [[nodiscard]] size_t max_buffers() const noexcept { return max_buffers_; }

    asio::io_context& context() noexcept { return context_; }

    void hand_start(uint32_t socket_id) const;

    void hand_stop(uint32_t socket_id, const std::error_code& ec) const;

    void hand_read(uint32_t socket_id, const uint8_t* data, size_t len) const;

    void hand_accept(uint32_t socket_id, uint32_t accepted, const std::string& local, const std::string& remote) const;

    void insert(uint32_t socket_id, const socket_base_ptr& ptr);

    void erase(uint32_t socket_id);

    using start_handle = std::function<void(uint32_t)>;

    void register_start_handle(start_handle&& handler) { start_ = std::move(handler); }

    using stop_handle = std::function<void(uint32_t, const std::error_code&)>;

    void register_stop_handle(stop_handle&& handler) { stop_ = std::move(handler); }

    using read_handle = std::function<void(uint32_t, const uint8_t*, size_t)>;

    void register_read_handle(read_handle&& handler) { read_ = std::move(handler); }

    using accept_handle = std::function<void(uint32_t, uint32_t, const std::string&, const std::string&)>;

    void register_accept_handle(accept_handle&& handler) { accept_ = std::move(handler); }

    using signal_callback = std::function<void(int)>;

    void register_signal_callback(signal_callback&& handler) {
        std::scoped_lock lock(mutex_single_);
        single_.emplace_back(std::move(handler));
    }

  private:
    socket_base_ptr find(uint32_t socket_id);

    asio::io_context context_;
    std::size_t max_buffers_{1};

    asio::signal_set signals_;
    std::thread thread_;

    std::unordered_map<uint32_t, socket_base_ptr> sockets_;
    std::mutex mutex_sockets_;

    std::atomic_uint32_t socket_ids_[socket_type_mask + 1];

    start_handle start_;
    stop_handle stop_;
    read_handle read_;
    accept_handle accept_;

    std::vector<signal_callback> single_;
    std::mutex mutex_single_;
};

}  // namespace simple
