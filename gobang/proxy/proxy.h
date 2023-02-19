#pragma once

#include <simple/web/websocket.h>

#include <random>
#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <unordered_set>
#include <vector>

struct socket_data {
    uint32_t socket{0};
    mutable int64_t last_recv{0};
    // 玩家id
    mutable int32_t userid{0};
    // 分配给玩家的逻辑服务器id
    mutable uint16_t logic{0};
    // 玩家当前所在的牌局服务器
    mutable uint16_t room{0};

    bool operator==(const socket_data& other) const { return socket == other.socket; }

    bool operator==(const uint32_t& other) const { return socket == other; }
};

template <>
struct std::hash<socket_data> {
    using is_transparent [[maybe_unused]] = int;

    [[nodiscard]] size_t operator()(const uint32_t& id) const noexcept { return std::hash<uint32_t>()(id); }
    [[nodiscard]] size_t operator()(const socket_data& data) const noexcept { return std::hash<uint32_t>()(data.socket); }
};

class gate_connector;

namespace game {
class s_service_info;
}

class proxy final : public simple::service_base {
  public:
    explicit proxy(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(proxy)

    ~proxy() noexcept override = default;

    simple::task<> awake() override;

  private:
    simple::task<> accept(uint32_t server);

    simple::task<> socket_start(uint32_t socket);

    simple::task<> socket_check(uint32_t socket);

    simple::task<> subscribe_login();

    void update_login(const game::s_service_info& service);

    uint16_t rand_login();

    void forward_gate([[maybe_unused]] uint32_t socket, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

    void forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

    void forward_player(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    // 监听端口
    uint16_t listen_port_;
    // 网络id -> socket_data
    std::unordered_set<socket_data, std::hash<socket_data>, std::equal_to<>> sockets_;
    // 连接gate
    std::shared_ptr<gate_connector> gate_connector_;

    struct login {
        uint16_t id;
        bool online;
    };
    std::vector<login> logins_;
    std::default_random_engine engine_;
};
