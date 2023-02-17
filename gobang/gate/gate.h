#pragma once

#include <simple/coro/condition_variable.h>
#include <simple/shm/shm_channel.h>

#include <deque>
#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct remote_gate;

namespace game {
class s_gate_forward_brd;
class s_service_info;
class s_service_register_req;
class s_gate_info;
}  // namespace game

struct channel_cached {
    channel_cached(std::string_view src, std::string_view dst, size_t size) : channel(src, dst, size) {}

    void write(const void* buf, uint32_t len);

    void auto_write();

    simple::shm_channel channel;
    std::deque<simple::memory_buffer_ptr> send_queue;
    simple::condition_variable cv_send_queue;
};

struct service_data {
    uint16_t id{0};
    uint16_t tp{0};

    uint16_t gate{0};
    union {
        // 服务在其他的gate机器上
        const remote_gate* remote{nullptr};
        // 服务在本地gate机器上
        channel_cached* local;
    };

    mutable bool online{false};

    bool operator==(const service_data& other) const { return id == other.id; }

    bool operator==(const uint16_t& other) const { return id == other; }

    void to_proto(game::s_service_info& info) const noexcept;
};

template <>
struct std::hash<service_data> {
    using is_transparent [[maybe_unused]] = int;

    [[nodiscard]] size_t operator()(const uint16_t& id) const noexcept { return std::hash<uint16_t>()(id); }
    [[nodiscard]] size_t operator()(const service_data& data) const noexcept { return std::hash<uint16_t>()(data.id); }
};

class remote_connector;

struct remote_gate {
    uint16_t id{0};

    mutable std::shared_ptr<remote_connector> connector;

    mutable std::vector<std::string> addresses;
    // 同机器上的所有服务
    mutable std::vector<const service_data*> services;

    bool operator==(const remote_gate& other) const { return id == other.id; }

    bool operator==(const uint16_t& other) const { return id == other; }
};

template <>
struct std::hash<remote_gate> {
    using is_transparent [[maybe_unused]] = int;

    [[nodiscard]] size_t operator()(const uint16_t& id) const noexcept { return std::hash<uint16_t>()(id); }
    [[nodiscard]] size_t operator()(const remote_gate& data) const noexcept { return std::hash<uint16_t>()(data.id); }
};

struct service_type_info {
    std::vector<const service_data*> services;
    std::vector<uint16_t> subscribe;
};

struct shm_header;
class master_connector;
class remote_listener;
class local_listener;

class gate final : public simple::service_base {
  public:
    explicit gate(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(gate)

    ~gate() noexcept override = default;

    simple::task<> awake() override;

    [[nodiscard]] const auto& master_address() const noexcept { return master_address_; }

    [[nodiscard]] const auto& remote_hosts() const noexcept { return remote_hosts_; }

    [[nodiscard]] auto remote_port() const noexcept { return remote_port_; }

    [[nodiscard]] auto local_port() const noexcept { return local_port_; }

    [[nodiscard]] auto& local_services() const noexcept { return local_services_; }

    void forward(game::s_gate_forward_brd brd);

    void forward(const std::string_view& message);

    void forward(const service_data* dest);

    void delay_forward(uint16_t dest, game::s_gate_forward_brd brd);

    service_type_info& get_service_type_info(uint16_t tp);

    simple::task<> local_service_online_changed(uint16_t service, bool online);

    simple::task<int32_t> upload_to_master(const game::s_service_info& info);

    const service_data* add_local_service(const game::s_service_register_req& req);

    void add_remote_gate(const game::s_gate_info& gate_info);

    const service_data* add_remote_service(const remote_gate& remote, const game::s_service_info& info);

  private:
    // 连接gate master的地址
    std::string master_address_;

    // 本机器其他服务连接的端口
    uint16_t local_port_{0};

    // 远程访问的端口,其他gate访问
    uint16_t remote_port_{0};
    // 给其他gate连接的ip地址或域名
    std::vector<std::string> remote_hosts_;

    // 找不到归属的服务，可能还没注册上来，先缓存下来，对这个队列设置个上限，超过就抛弃之前的
    // 一般只会在启动时候才会出现，刚开始启动不会有很多消息
    using send_queue = std::deque<std::shared_ptr<game::s_gate_forward_brd>>;
    std::unordered_map<uint16_t, send_queue> send_queues_;

    // gate 的service id —> gate_data
    std::unordered_set<remote_gate, std::hash<remote_gate>, std::equal_to<>> remote_gates_;
    // 其他服务的 service id —> service_data*
    std::unordered_set<service_data, std::hash<service_data>, std::equal_to<>> services_;
    // 本地的服务
    std::vector<const service_data*> local_services_;
    // 本地服务的共享内存通道
    std::vector<std::unique_ptr<channel_cached>> local_channels_;
    // 服务分类
    std::unordered_map<uint16_t, service_type_info> service_type_infos_;
    // 本地服务用到的共享内存
    std::unordered_map<std::string, simple::shm> service_shm_map_;
    // 连接gate master
    std::shared_ptr<master_connector> master_connector_;
    // 监听其他gate的连接
    std::shared_ptr<remote_listener> remote_listener_;
    // 监听本机器其他的服务
    std::shared_ptr<local_listener> local_listener_;
};
