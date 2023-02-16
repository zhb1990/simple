#pragma once
#include <fmt/format.h>
#include <simple/config.h>
#include <simple/net/socket_types.h>
#include <simple/utils/time.h>

#include <asio/ip/basic_endpoint.hpp>
#include <simple/containers/buffer.hpp>

namespace simple {

class socket_base {
  public:
    explicit socket_base(uint32_t socket_id) : socket_id_(socket_id) {}

    SIMPLE_NON_COPYABLE(socket_base)

    virtual ~socket_base() noexcept = default;

    virtual void stop(const std::error_code& ec) = 0;

    virtual void write(const memory_buffer_ptr& ptr) {}

    virtual void accept() {}

    virtual void no_delay(bool on) {}

    void trace_write(int64_t size) {
        trace_.write += size;
        trace_.write_time = get_system_clock_millis();
    }

    void trace_read(int64_t size) {
        trace_.read += size;
        trace_.read_time = get_system_clock_millis();
    }

    void trace_write_queue(int64_t size) { trace_.write_queue += size; }

  protected:
    uint32_t socket_id_;
    socket_trace trace_;
};

template <typename InternetProtocol>
inline std::string to_string(asio::ip::basic_endpoint<InternetProtocol>& endpoint) {
    try {
        auto address = endpoint.address();
        if (address.is_v4()) {
            return fmt::format("{}:{}", address.to_string(), endpoint.port());
        }
        // ipv6
        return fmt::format("[{}]:{}", address.to_string(), endpoint.port());
    } catch (...) {
        return {};
    }
}

}  // namespace simple
