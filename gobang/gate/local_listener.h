#pragma once

// 给本机器上的其他服务注册用，用来判断是否离线
#include <google/protobuf/message.h>

#include <list>
#include <optional>
#include <simple/application/event_system.hpp>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <simple/utils/toml_types.hpp>
#include <unordered_map>
#include <vector>

#include "local_service.h"

namespace game {
class s_service_register_req;
}  // namespace game

class local_listener {
  public:
    local_listener(simple::service& s, const simple::toml_table_t& table);

    SIMPLE_NON_COPYABLE(local_listener)

    ~local_listener() noexcept = default;

    simple::task<> start();

    void update_service(const service_update_event& data);

    std::vector<service_info*> local_services();

  private:
    simple::task<> accept(uint32_t server);

    struct socket_data {
        uint32_t socket{0};
        int64_t last_recv{0};
        local_service* service{nullptr};
        // 订阅的类型
        std::vector<uint16_t> subscribe;
    };

    using socket_ptr = std::shared_ptr<socket_data>;

    simple::task<> socket_start(const socket_ptr& ptr);

    [[nodiscard]] simple::task<> socket_check(const socket_ptr& ptr) const;

    void forward_message(const socket_ptr& ptr, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    void service_register(const socket_ptr& ptr, uint64_t session, const simple::memory_buffer& buffer);

    void service_subscribe(const socket_ptr& ptr, uint64_t session, const simple::memory_buffer& buffer);

    static void send(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg);

    local_service* add_local_service(const game::s_service_register_req& req);

    simple::service& service_;
    // 本机器其他服务连接的端口
    uint16_t local_port_{0};
    std::list<local_service> services_;
    // tp -> 所有订阅了的socket data
    std::unordered_map<uint16_t, std::vector<socket_ptr>> subscribe_infos_;
    // 本地服务用到的共享内存
    std::unordered_map<std::string, simple::shm> service_shm_map_;
    std::optional<simple::event_registration> update_service_;
};
