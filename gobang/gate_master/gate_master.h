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
    bool online{false};
};

struct gate_data {
    uint16_t id{0};
    uint32_t socket{0};
    google::protobuf::RepeatedPtrField<game::s_gate_address> addresses;
    // 同机器上的所有服务
    std::vector<service_data*> services;
};

struct socket_data {
    uint32_t socket{0};
    gate_data* data{nullptr};
    int64_t last_recv{0};
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

    void gate_disconnect(gate_data* gate) const;

    void publish(gate_data* gate) const;

    static void send(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg);

    void forward_message(socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    // gate 注册 并返回所有的进程信息
    void gate_register(socket_data& socket, uint64_t session, const simple::memory_buffer& buffer);

    // gate 上报当前机器上的其他进程变更
    void gate_upload(socket_data& socket, uint64_t session, const simple::memory_buffer& buffer);

    bool check_services(const google::protobuf::RepeatedPtrField<game::s_service_info>& services, uint16_t gate);

    void add_services(const google::protobuf::RepeatedPtrField<game::s_service_info>& services, gate_data* gate);

    // gate 的service id —> gate_data
    std::unordered_map<uint16_t, gate_data> gates_;
    // 其他服务的 service id —> service_data*
    std::unordered_map<uint16_t, service_data> services_;
    // 网络id -> socket_data
    std::unordered_map<uint32_t, socket_data> sockets_;
    // 监听端口
    uint16_t listen_port_{0};
};
