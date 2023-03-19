#pragma once

#include <google/protobuf/message.h>
#include <proto_rpc.h>
#include <simple/coro/network.h>
#include <simple/shm/shm_channel.h>

#include <headers.hpp>
#include <kernel.hpp>
#include <string>

KERNEL_API simple::memory_buffer_ptr create_net_buffer(uint16_t id, uint64_t session, const google::protobuf::Message& msg);

KERNEL_API void init_forward_buffer(simple::memory_buffer& buf, const forward_part& part, const google::protobuf::Message& msg);

KERNEL_API void init_forward_buffer(simple::memory_buffer& buf, uint16_t from, uint16_t to, uint64_t session,
                                    const client_part& client, const google::protobuf::Message& msg);

KERNEL_API void init_client_buffer(simple::memory_buffer& buf, uint16_t id, uint64_t session,
                                   const google::protobuf::Message& msg);

KERNEL_API simple::task<> recv_net_buffer(simple::memory_buffer& buf, net_header& header, uint32_t socket);

KERNEL_API void proc_ping(uint32_t socket, uint64_t session, const simple::memory_buffer& buffer);

KERNEL_API simple::task<int64_t> rpc_ping(rpc_system& system, uint32_t socket);

KERNEL_API int64_t connect_interval(size_t fail_count);

template <std::derived_from<google::protobuf::Message> Message>
simple::task<Message> rpc_call(rpc_system& system, uint32_t socket, uint16_t id, const google::protobuf::Message& req) {
    const auto session = system.create_session();
    const auto buf = create_net_buffer(id, session, req);
    simple::network::instance().write(socket, buf);
    co_return co_await system.get_awaiter<Message>(session);
}

template <std::derived_from<google::protobuf::Message> Message>
simple::task<Message> rpc_call(rpc_system& system, simple::shm_channel& channel, uint16_t from, uint16_t to, uint16_t id,
                               const google::protobuf::Message& req) {
    const auto session = system.create_session();
    simple::memory_buffer buf;
    init_forward_buffer(buf, {from, to, id, 0, session}, req);
    co_await channel.write(buf.begin_read(), buf.readable());
    co_return co_await system.get_awaiter<Message>(session);
}
