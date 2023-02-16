#pragma once

#include <simple/config.h>

#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>

namespace simple {

enum class websocket_type { client, server };

enum class websocket_opcode : uint8_t {
    continuation = 0x0,
    text = 0x1,
    binary = 0x2,
    rsv3 = 0x3,
    rsv4 = 0x4,
    rsv5 = 0x5,
    rsv6 = 0x6,
    rsv7 = 0x7,
    close = 0x8,
    ping = 0x9,
    pong = 0xA,
    control_rsv_b = 0xB,
    control_rsv_c = 0xC,
    control_rsv_d = 0xD,
    control_rsv_e = 0xE,
    control_rsv_f = 0xF,
    none = 0x10
};

constexpr bool is_websocket_control(websocket_opcode op) { return static_cast<uint8_t>(op) >= 0x8; }

class websocket {
  public:
    SIMPLE_API websocket(websocket_type tp, uint32_t socket);

    SIMPLE_COPYABLE_DEFAULT(websocket)

    ~websocket() noexcept = default;

    // 客户端握手
    [[nodiscard]] SIMPLE_API simple::task<> handshake(std::string_view host, std::string_view uri = "/") const;

    // 服务端握手
    [[nodiscard]] SIMPLE_API simple::task<> handshake() const;

    // 握手成功后接收websocket消息
    [[nodiscard]] SIMPLE_API simple::task<websocket_opcode> read(memory_buffer& buf) const;

    // 握手成功后发送websocket消息
    SIMPLE_API void write(websocket_opcode op, const void* data, size_t size) const;

    // 握手成功后发送websocket消息, 将要发送的消息拆分成多个帧，payload_max 为单帧最大的消息长度
    SIMPLE_API void write(websocket_opcode op, const void* data, size_t size, size_t payload_max) const;

    [[nodiscard]] auto socket() const noexcept { return socket_; }

  private:
    void encode_header(memory_buffer& buf, websocket_opcode op, size_t size, bool fin) const;

    void encode_body(memory_buffer& buf, websocket_opcode op, const void* data, size_t size) const;

    websocket_type tp_;
    uint32_t socket_;
};

}  // namespace simple
