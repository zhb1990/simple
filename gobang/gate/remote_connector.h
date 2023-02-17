#pragma once
#include <proto_rpc.h>
#include <simple/coro/condition_variable.h>

#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>

// gate 与 gate 两两互联

class gate;
struct remote_gate;

class remote_connector {
  public:
    explicit remote_connector(const remote_gate* remote, gate& g);

    SIMPLE_NON_COPYABLE(remote_connector)

    ~remote_connector() noexcept = default;

    void start();

    void send(simple::memory_buffer_ptr ptr);

  private:
    simple::task<> run();

    simple::task<> auto_ping(uint32_t socket);

    simple::task<> auto_send(uint32_t socket);

    simple::task<> ping_to_remote(uint32_t socket);

    const remote_gate* remote_;
    gate& gate_;
    rpc_system system_;
    uint32_t socket_{0};
    simple::condition_variable cv_send_queue_;
    std::deque<simple::memory_buffer_ptr> send_queue_;
};
