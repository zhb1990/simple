#pragma once

#include <proto_utils.h>
#include <simple/coro/condition_variable.h>
#include <simple/shm/shm_channel.h>

#include <deque>
#include <functional>
#include <random>
#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <vector>

namespace game {
class s_service_info;
}

// 订阅的服务信息，现在只有一个id和是否在线
struct service_info_subscribe {
    uint16_t id;
    bool online;
};

// 其他服务连接 gate
class gate_connector {
  public:
    using fn_on_register = std::function<simple::task<>()>;
    // void forward(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);
    using fn_forward = std::function<void(uint16_t, uint64_t, uint16_t, const simple::memory_buffer&)>;

    using shm_infos = std::vector<std::pair<std::string, uint32_t>>;

    KERNEL_API explicit gate_connector(simple::service& service, const simple::toml_value_t* value, int32_t service_type,
                                       fn_on_register on_register, fn_forward forward, shm_infos infos = {});

    SIMPLE_NON_COPYABLE(gate_connector)

    ~gate_connector() noexcept = default;

    KERNEL_API void start();

    KERNEL_API void write(uint16_t to, uint64_t session, uint16_t id, const google::protobuf::Message& msg);

    KERNEL_API void write(uint16_t to, uint64_t session, const client_part& client, const google::protobuf::Message& msg);

    KERNEL_API void write(simple::memory_buffer& msg);

    template <std::invocable<simple::memory_buffer&> Init>
    void write(Init&& init);

    template <std::derived_from<google::protobuf::Message> Message>
    simple::task<Message> call_gate(uint16_t id, const google::protobuf::Message& req) {
        return rpc_call<Message>(system_, socket_, id, req);
    }

    template <std::derived_from<google::protobuf::Message> Message>
    simple::task<Message> call(uint16_t to, uint16_t id, const google::protobuf::Message& req) {
        const auto session = system_.create_session();
        write(to, session, id, req);
        co_return co_await system_.get_awaiter<Message>(session);
    }

    KERNEL_API simple::task<> subscribe(uint16_t tp);

    [[nodiscard]] KERNEL_API const std::vector<service_info_subscribe>* find_subscribe(uint16_t tp) const;

    KERNEL_API uint16_t rand_subscribe(uint16_t tp);

  private:
    simple::task<> run();

    simple::task<> recv_one_message(uint32_t socket);

    simple::task<> auto_ping(uint32_t socket);

    simple::task<bool> register_to_gate(uint32_t socket);

    simple::task<> ping_to_gate(uint32_t socket);

    simple::task<> channel_read();

    simple::task<> channel_write();

    void forward_gate(uint16_t id, const simple::memory_buffer& buffer);

    void update_subscribe(const game::s_service_info& service);

    simple::service& service_;
    int32_t service_type_;
    uint16_t port_;
    uint32_t channel_size_;
    fn_on_register on_register_;
    fn_forward forward_;
    uint32_t socket_{0};
    rpc_system system_;
    std::unique_ptr<simple::shm_channel> channel_;
    std::deque<simple::memory_buffer_ptr> send_queue_;
    simple::condition_variable cv_send_queue_;
    shm_infos shm_infos_;
    // 所有订阅类型的服务
    std::unordered_map<uint16_t, std::vector<service_info_subscribe>> subscribes_;
    std::default_random_engine engine_;
};
