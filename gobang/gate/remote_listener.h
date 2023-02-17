#pragma once

#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>

class gate;

class remote_listener {
  public:
    explicit remote_listener(gate& g);

    SIMPLE_NON_COPYABLE(remote_listener)

    ~remote_listener() noexcept = default;

    simple::task<> start();

  private:
    simple::task<> accept(uint32_t server);

    struct socket_data {
        uint32_t socket{0};
        int64_t last_recv{0};
    };

    using socket_data_ptr = std::shared_ptr<socket_data>;

    simple::task<> socket_start(const socket_data_ptr& ptr);

    simple::task<> socket_check(const socket_data_ptr& ptr);

    void forward_message(uint32_t socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    void gate_forward(uint32_t socket, const simple::memory_buffer& buffer);

    gate& gate_;
};
