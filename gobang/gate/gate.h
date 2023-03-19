#pragma once
#include <deque>
#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "service_info.h"

class master_connector;
class remote_listener;
class local_listener;

class gate final : public simple::service {
  public:
    explicit gate(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(gate)

    ~gate() noexcept override = default;

    simple::task<> awake() override;

    service_info* find_service(uint16_t id);

    void emplace_service(service_info* info);

    const std::vector<service_info*>* find_service_type(uint16_t tp);

    void forward(const forward_message_event& ev);

  private:
    // 找不到归属的服务，可能还没注册上来，先缓存下来，对这个队列设置个上限，超过就抛弃之前的
    // 一般只会在启动时候才会出现，刚开始启动不会有很多消息
    using send_queue = std::deque<simple::memory_buffer>;
    std::unordered_map<uint16_t, send_queue> send_queues_;

    // 其他服务的 service id —> service_info*
    std::unordered_map<uint16_t, service_info*> services_;
    // tp -> 同类型的所有 service id 如果需要分服分区，那还需要加个组id进行归类
    std::unordered_map<uint16_t, std::vector<service_info*>> service_types_;

    // 连接gate master
    std::shared_ptr<master_connector> master_connector_;
    // 监听其他gate的连接
    std::shared_ptr<remote_listener> remote_listener_;
    // 监听本机器其他的服务
    std::shared_ptr<local_listener> local_listener_;
};
