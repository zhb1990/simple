#pragma once

#include <msg_server.pb.h>
#include <simple/web/websocket.h>

#include <deque>
#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct socket_data {
    uint32_t socket{0};
    // 玩家id
    mutable int32_t userid{0};
    // 分配给玩家的逻辑服务器id
    mutable uint16_t logic{0};
    // 玩家当前所在的牌局服务器
    mutable uint16_t room{0};
    // 是否已经收到登录注册协议
    mutable bool wait_login{false};
    mutable int64_t last_recv{0};
    mutable std::deque<game::s_client_forward_brd> cache;

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

    simple::task<> on_register_to_gate();

    void forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

    void forward_client(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    void client_register_msg(const socket_data& socket, uint64_t session, const simple::memory_buffer& buffer);

    void client_room_msg(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    void client_other_msg(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    void send_to_service(uint16_t dest_service, uint32_t socket, uint16_t id, uint64_t session,
                         const simple::memory_buffer& buffer);

    // 直接发给客户端
    void send_to_client(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg);

    // 内部服务发过来的 需要转发给客户端的 消息
    void client_forward_brd(const simple::memory_buffer& buffer);

    void report_client_offline(const socket_data& socket);

    void kick_client(uint16_t from, uint64_t session, const simple::memory_buffer& buffer);

    void client_login_ack(const socket_data& socket, const game::s_client_forward_brd& brd);

    static void client_match_ack(const socket_data& socket, const game::s_client_forward_brd& brd);

    static void client_move_ack(const socket_data& socket, const game::s_client_forward_brd& brd);

    static void client_move_brd(const socket_data& socket, const game::s_client_forward_brd& brd);

    // 监听端口
    uint16_t listen_port_;
    // 网络id -> socket_data
    std::unordered_set<socket_data, std::hash<socket_data>, std::equal_to<>> sockets_;
    // 连接gate
    std::shared_ptr<gate_connector> gate_connector_;
    using fn_client_msg = std::function<void(const socket_data&, uint64_t, const simple::memory_buffer&)>;
    std::unordered_map<uint16_t, fn_client_msg> fn_client_msgs_;
    simple::memory_buffer temp_buffer_;
    using fn_on_client_forward_brd = std::function<void(const socket_data&, const game::s_client_forward_brd&)>;
    std::unordered_map<uint16_t, fn_on_client_forward_brd> fn_on_client_forward_brd_;
};
