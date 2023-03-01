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

void init_client_buffer(simple::memory_buffer& buf, uint16_t id, uint64_t session, const google::protobuf::Message& msg) {
    const ws_header header{flag_valid, {}, id, session};
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

    if (const auto len = header.len; len > msg_len_limit) {
        network.close(socket);
        throw std::logic_error(fmt::format("header len:{} > msg_len_limit", len));
    }

    if (header.len > 0) {
        buf.reserve(header.len);
        recv_len = co_await network.read_size(socket, reinterpret_cast<char*>(buf.begin_write()), header.len);
        if (recv_len == 0) {
            throw std::logic_error("recv eof");
        }

        buf.written(recv_len);
    }
}

void proc_ping(uint32_t socket, uint64_t session, const simple::memory_buffer& buffer) {
    game::msg_empty req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        return;
    }

    // 不填result，即为默认的success
    const auto buf = create_net_buffer(game::id_s_ping_ack, session, game::msg_common_ack{});
    simple::network::instance().write(socket, buf);
}

simple::task<int64_t> rpc_ping(rpc_system& system, uint32_t socket) {
    const auto last = simple::get_system_clock_millis();
    co_await rpc_call<game::msg_common_ack>(system, socket, game::id_s_ping_req, game::msg_empty{});
    co_return simple::get_system_clock_millis() - last;
}

int64_t connect_interval(size_t fail_count) {
    if (fail_count < 2) {
        return 0;
    }

    constexpr int64_t intervals[] = {1, 2, 4, 6, 8};
    constexpr size_t size = std::size(intervals);
    if (const auto idx = fail_count - 2; idx >= size) {
        return intervals[size - 1];
    } else {
        return intervals[idx];
    }
}
