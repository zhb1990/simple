#pragma once
#include <msg_server.pb.h>

#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <string>
#include <unordered_set>
#include <vector>

struct service_data {
    uint16_t id{0};
    uint16_t tp{0};
    uint16_t gate{0};
    mutable bool online{false};

    bool operator==(const service_data& other) const { return id == other.id; }

    bool operator==(const uint16_t& other) const { return id == other; }
};

template <>
struct std::hash<service_data> {
    using is_transparent [[maybe_unused]] = int;

    [[nodiscard]] size_t operator()(const uint16_t& id) const noexcept { return std::hash<uint16_t>()(id); }
    [[nodiscard]] size_t operator()(const service_data& data) const noexcept { return std::hash<uint16_t>()(data.id); }
};

struct gate_data {
    uint16_t id{0};
    mutable uint32_t socket{0};
    mutable google::protobuf::RepeatedPtrField<game::s_gate_address> addresses;
    // 同机器上的所有服务
    mutable std::vector<const service_data*> services;

    bool operator==(const gate_data& other) const { return id == other.id; }

    bool operator==(const uint16_t& other) const { return id == other; }
};

template <>
struct std::hash<gate_data> {
    using is_transparent [[maybe_unused]] = int;

    [[nodiscard]] size_t operator()(const uint16_t& id) const noexcept { return std::hash<uint16_t>()(id); }
    [[nodiscard]] size_t operator()(const gate_data& data) const noexcept { return std::hash<uint16_t>()(data.id); }
};

struct socket_data {
    uint32_t socket{0};
    mutable const gate_data* data{nullptr};
    mutable int64_t last_recv{0};

    bool operator==(const socket_data& other) const { return socket == other.socket; }

    bool operator==(const uint32_t& other) const { return socket == other; }
};

template <>
struct std::hash<socket_data> {
    using is_transparent [[maybe_unused]] = int;

    [[nodiscard]] size_t operator()(const uint32_t& id) const noexcept { return std::hash<uint32_t>()(id); }
    [[nodiscard]] size_t operator()(const socket_data& data) const noexcept { return std::hash<uint32_t>()(data.socket); }
};

class gate_master final : public simple::service {
  public:
    explicit gate_master(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(gate_master)

    ~gate_master() noexcept override = default;

    simple::task<> awake() override;

  private:
    simple::task<> accept(uint32_t server);

    simple::task<> socket_start(uint32_t socket);

    simple::task<> socket_check(uint32_t socket);

    void gate_disconnect(const gate_data* gate) const;

    void publish(const gate_data* gate) const;

    static void send(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg);

    void forward_message(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    // gate 注册 并返回所有的进程信息
    void gate_register(const socket_data& socket, uint64_t session, const simple::memory_buffer& buffer);

    // gate 上报当前机器上的其他进程变更
    void gate_upload(const socket_data& socket, uint64_t session, const simple::memory_buffer& buffer);

    bool check_services(const google::protobuf::RepeatedPtrField<game::s_service_info>& services, uint16_t gate);

    void add_services(const google::protobuf::RepeatedPtrField<game::s_service_info>& services, const gate_data* gate);

    // gate 的service id —> gate_data
    std::unordered_set<gate_data, std::hash<gate_data>, std::equal_to<>> gates_;
    // 其他服务的 service id —> service_data*
    std::unordered_set<service_data, std::hash<service_data>, std::equal_to<>> services_;
    // 网络id -> socket_data
    std::unordered_set<socket_data, std::hash<socket_data>, std::equal_to<>> sockets_;
    // 监听端口
    uint16_t listen_port_{0};
};
