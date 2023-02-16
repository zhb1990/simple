#include "proto_utils.h"

#include <fmt/format.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <simple/utils/time.h>

#include <cstring>
#include <stdexcept>

simple::memory_buffer_ptr create_net_buffer(uint16_t id, uint64_t session, const google::protobuf::Message& msg) {
    net_header header{flag_valid, 0, id, 0, session};
    header.len = static_cast<uint32_t>(msg.ByteSizeLong());
    auto buf = std::make_shared<simple::memory_buffer>();
    buf->reserve(header.len + sizeof(header));
    buf->append(&header, sizeof(header));
    msg.SerializePartialToArray(buf->begin_write(), static_cast<int>(buf->writable()));
    buf->written(header.len);
    return buf;
}

void init_shm_buffer(simple::memory_buffer& buf, const shm_header& header, const google::protobuf::Message& msg) {
    const auto len = static_cast<uint32_t>(msg.ByteSizeLong());
    buf.reserve(sizeof(header) + len);
    buf.append(&header, sizeof(header));
    msg.SerializePartialToArray(buf.begin_write(), static_cast<int>(buf.writable()));
    buf.written(len);
}

simple::task<> recv_net_buffer(simple::memory_buffer& buf, net_header& header, uint32_t socket) {
    buf.clear();
    memset(&header, 0, sizeof(header));
    auto& network = simple::network::instance();
    // 限制一个消息最大10M
    constexpr uint32_t msg_len_limit = 1024 * 1024 * 10;
    auto recv_len = co_await network.read_size(socket, reinterpret_cast<char*>(&header), sizeof(header));
    if (recv_len == 0) {
        throw std::logic_error("recv eof");
    }

    if (!header.valid()) {
        network.close(socket);
        auto flag = header.flag;
        throw std::logic_error(fmt::format("header flag:{} is invalid", flag));
    }

    const auto len = header.len;
    if (len > msg_len_limit) {
        network.close(socket);
        throw std::logic_error(fmt::format("header len:{} > msg_len_limit", len));
    }

    buf.reserve(header.len);
    recv_len = co_await network.read_size(socket, reinterpret_cast<char*>(buf.begin_write()), header.len);
    if (recv_len == 0) {
        throw std::logic_error("recv eof");
    }

    buf.written(recv_len);
}

void proc_ping(uint32_t socket, uint64_t session, const simple::memory_buffer& buffer) {
    game::s_ping_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        return;
    }

    game::s_ping_ack ack;
    ack.set_t1(req.t1());
    ack.set_t2(simple::get_system_clock_millis());
    auto buf = create_net_buffer(game::id_s_ping_ack, session, ack);
    simple::network::instance().write(socket, buf);
}

simple::task<int64_t> rpc_ping(rpc_system& system, uint32_t socket) {
    game::s_ping_req req;
    const auto last = simple::get_system_clock_millis();
    req.set_t1(last);
    [[maybe_unused]] const auto ack = co_await rpc_call<game::s_ping_ack>(system, socket, game::id_s_ping_req, req);
    co_return simple::get_system_clock_millis() - last;
}
