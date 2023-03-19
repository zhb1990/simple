#pragma once
#include <proto_rpc.h>

#include <optional>
#include <simple/application/event_system.hpp>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <simple/utils/toml_types.hpp>
#include <unordered_map>

#include "remote_gate.h"

namespace game {
class s_gate_info;
}

// 连接 gate master
class master_connector {
  public:
    master_connector(simple::service& s, const simple::toml_table_t& table);

    SIMPLE_NON_COPYABLE(master_connector)

    ~master_connector() noexcept = default;

    void start();

    simple::task<int32_t> upload(const game::s_service_info& info);

    void update_service(const service_update_event& data);

    std::shared_ptr<simple::task<int32_t>> upload_to_master(const game::s_service_info& info);

  private:
    simple::task<> run();

    simple::task<> recv_one_message(uint32_t socket);

    simple::task<> auto_ping(uint32_t socket);

    simple::task<bool> register_to_master(uint32_t socket);

    void add_remote_gate(const game::s_gate_info& info);

    simple::task<> ping_to_master(uint32_t socket);

    void forward_message(uint16_t id, const simple::memory_buffer& buffer);

    simple::service& service_;
    uint32_t socket_{0};
    rpc_system system_;
    // service id —> remote_gate
    std::unordered_map<uint16_t, remote_gate> remote_gates_;
    // 连接gate master的地址
    std::string master_address_;
    // 给其他gate连接的ip地址或域名
    std::vector<std::string> remote_hosts_;
    uint16_t remote_port_{0};
    std::optional<simple::event_registration> update_service_;
};
