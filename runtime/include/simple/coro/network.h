#pragma once
#include <simple/config.h>

#include <chrono>
#include <memory>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <unordered_map>

namespace simple {

struct network_data;

using network_data_ptr = std::shared_ptr<network_data>;

class network_awaiter;

// 只能协程中使用的网络模块

class network {
    network() = default;

  public:
    ~network() noexcept = default;

    DS_NON_COPYABLE(network)

    DS_API static network& instance();

    DS_API task<uint32_t> tcp_listen(const std::string& host, uint16_t port, bool reuse);

    DS_API task<uint32_t> ssl_listen(const std::string& host, uint16_t port, bool reuse, const std::string& cert,
                                     const std::string& key, const std::string& dh, const std::string& password);

    DS_API task<uint32_t> kcp_listen(const std::string& host, uint16_t port, bool reuse);

    DS_API task<uint32_t> tcp_connect(const std::string& host, const std::string& service,
                                      const std::chrono::milliseconds& timeout);

    DS_API task<uint32_t> ssl_connect(const std::string& host, const std::string& service,
                                      const std::chrono::milliseconds& timeout, const std::string& verify = "",
                                      bool ignore_cert = true);

    DS_API task<uint32_t> kcp_connect(const std::string& host, const std::string& service,
                                      const std::chrono::milliseconds& timeout);

    DS_API task<uint32_t> accept(uint32_t listen_id);

    DS_API task<size_t> read(uint32_t socket_id, void* buf, size_t size);

    DS_API task<size_t> read_size(uint32_t socket_id, void* buf, size_t size);

    DS_API task<memory_buffer_ptr> read_until(uint32_t socket_id, std::string_view end);

    DS_API task<size_t> read_until(uint32_t socket_id, std::string_view end, memory_buffer& buf);

    DS_API void write(uint32_t socket_id, const memory_buffer_ptr& buf);

    DS_API void close(uint32_t socket_id);

    DS_API void no_delay(uint32_t socket_id, bool on);

    DS_API std::string local_address(uint32_t socket_id);

    DS_API std::string remote_address(uint32_t socket_id);

    DS_API void init();

    bool remove_socket(uint32_t socket_id);

  private:
    void hand_start(uint32_t socket_id);

    void hand_stop(uint32_t socket_id, const std::error_code& ec);

    void hand_read(uint32_t socket_id, const memory_buffer& data);

    void hand_accept(uint32_t socket_id, uint32_t accepted, const std::string& local, const std::string& remote);

    template <typename Service>
    network_awaiter create_start_awaiter(uint32_t socket_id, const std::string& host, const Service& service);

    std::unordered_map<uint32_t, network_data_ptr> sockets_;

    std::atomic_bool init_{false};
};

}  // namespace simple
