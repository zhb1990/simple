#pragma once

#include <proto_utils.h>
#include <simple/coro/condition_variable.h>
#include <simple/shm/shm_channel.h>

#include <deque>
#include <functional>
#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <vector>

// 其他服务连接 gate
class gate_connector {
  public:
    // void forward_gate(uint32_t socket, uint64_t session, uint16_t id, const simple::memory_buffer& buffer)
    using forward_gate_fn = std::function<void(uint32_t, uint64_t, uint16_t, const simple::memory_buffer&)>;
    // void forward(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);
    using forward_fn = std::function<void(uint16_t, uint64_t, uint16_t, const simple::memory_buffer&)>;

    using shm_infos = std::vector<std::pair<std::string, uint32_t>>;

    KERNEL_API explicit gate_connector(simple::service_base& service, const simple::toml_value_t* value, int32_t service_type,
                                       forward_gate_fn forward_gate, forward_fn forward, shm_infos infos = {});

    SIMPLE_NON_COPYABLE(gate_connector)

    ~gate_connector() noexcept = default;

    KERNEL_API void start();

    [[nodiscard]] auto is_socket_valid() const noexcept { return socket_ > 0; }

    KERNEL_API void write(uint16_t to, uint64_t session, uint16_t id, const google::protobuf::Message& msg);

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

  private:
    simple::task<> run();

    simple::task<> recv_one_message(uint32_t socket);

    simple::task<> auto_ping(uint32_t socket);

    simple::task<bool> register_to_gate(uint32_t socket);

    simple::task<> ping_to_gate(uint32_t socket);

    simple::task<> channel_read();

    simple::task<> channel_write();

    simple::service_base& service_;
    int32_t service_type_;
    uint16_t port_;
    uint32_t channel_size_;
    forward_gate_fn forward_gate_;
    forward_fn forward_;
    uint32_t socket_{0};
    rpc_system system_;
    std::unique_ptr<simple::shm_channel> channel_;
    std::deque<simple::memory_buffer_ptr> send_queue_;
    simple::condition_variable cv_send_queue_;
    shm_infos shm_infos_;
};
