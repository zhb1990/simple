#pragma once

#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <simple/utils/toml_types.hpp>

#include "service_info.h"

class remote_listener {
  public:
    remote_listener(simple::service& s, const simple::toml_table_t& table);

    SIMPLE_NON_COPYABLE(remote_listener)

    ~remote_listener() noexcept = default;

    simple::task<> start();

  private:
    simple::task<> accept(uint32_t server);

    struct socket_data {
        uint32_t socket{0};
        int64_t last_recv{0};
    };

    using socket_ptr = std::shared_ptr<socket_data>;

    simple::task<> socket_start(const socket_ptr& ptr);

    [[nodiscard]] simple::task<> socket_check(const socket_ptr& ptr) const;

    void forward_message(uint32_t socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer) const;

    void gate_forward(const simple::memory_buffer& buffer) const;

    simple::service& service_;
    uint16_t remote_port_{0};
};
