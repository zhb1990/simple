#pragma once
#include <proto_rpc.h>

#include <deque>
#include <list>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>

#include "service_info.h"

class remote_gate;

struct remote_service final : service_info {
    remote_gate* remote{nullptr};

    void write(const std::string_view& message) override;
};

class remote_gate {
  public:
    remote_gate(simple::service& s, uint16_t id);

    SIMPLE_COPYABLE_DEFAULT(remote_gate)

    ~remote_gate() noexcept = default;

    void start();

    void send(simple::memory_buffer_ptr ptr);

    void set_addresses(std::vector<std::string> addresses);

	void add_remote_service(const game::s_service_info& info);

  private:
    simple::task<> run();

    simple::task<> auto_ping(uint32_t socket);

    void auto_send(uint32_t socket);

    simple::task<> ping_to_remote(uint32_t socket);

    simple::service* service_{nullptr};
    uint16_t id_{0};
    std::vector<std::string> addresses_;
    // 同机器上的所有服务
    std::list<remote_service> services_;

    rpc_system system_;
    uint32_t socket_{0};
    std::deque<simple::memory_buffer_ptr> send_queue_;
};
