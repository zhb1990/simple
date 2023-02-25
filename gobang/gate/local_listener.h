#pragma once

// 给本机器上的其他服务注册用，用来判断是否离线
#include <google/protobuf/message.h>

#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <unordered_map>

namespace game {
class s_service_register_req;
}  // namespace game

class gate;
struct service_data;

class local_listener {
  public:
    explicit local_listener(gate& g);

    SIMPLE_NON_COPYABLE(local_listener)

    ~local_listener() noexcept = default;

    simple::task<> start();

    void publish(const service_data* data);

  private:
    simple::task<> accept(uint32_t server);

    struct socket_data {
        uint32_t socket{0};
        uint16_t service{0};
        int64_t last_recv{0};
    };

    using socket_data_ptr = std::shared_ptr<socket_data>;

    simple::task<> socket_start(const socket_data_ptr& ptr);

    [[nodiscard]] simple::task<> socket_check(const socket_data_ptr& ptr) const;

    void forward_message(const socket_data_ptr& ptr, uint16_t id, uint64_t session, const simple::memory_buffer& buffer);

    void service_register(const socket_data_ptr& ptr, uint64_t session, const simple::memory_buffer& buffer);

    simple::task<> service_register(const socket_data_ptr& ptr, uint64_t session, const game::s_service_register_req& req);

    void service_subscribe(const socket_data_ptr& ptr, uint64_t session, const simple::memory_buffer& buffer) const;

    static void send(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg);

    gate& gate_;
    std::unordered_map<uint16_t, socket_data_ptr> service_sockets_;
};
