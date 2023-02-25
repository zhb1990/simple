#pragma once
#include <proto_rpc.h>

#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>

class gate;

namespace game {
class s_service_info;
}

// 连接 gate master
class master_connector {
  public:
    explicit master_connector(gate& g);

    SIMPLE_NON_COPYABLE(master_connector)

    ~master_connector() noexcept = default;

    void start();

    simple::task<int32_t> upload_to_master(const game::s_service_info& info);

  private:
    simple::task<> run();

    simple::task<> recv_one_message(uint32_t socket);

    simple::task<> auto_ping(uint32_t socket);

    simple::task<bool> register_to_master(uint32_t socket);

    simple::task<> ping_to_master(uint32_t socket);

    void forward_message(uint16_t id, const simple::memory_buffer& buffer) const;

    gate& gate_;
    uint32_t socket_{0};
    rpc_system system_;
};
