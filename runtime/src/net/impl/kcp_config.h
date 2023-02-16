#pragma once
#include <ikcp.h>

#include <chrono>
#include <cstdint>
#include <vector>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

namespace simple {

inline constexpr std::chrono::seconds kcp_alive_timeout(20);
inline constexpr std::chrono::seconds kcp_heartbeat_timeout(10);

inline constexpr uint8_t kcp_op_connect = 1;
inline constexpr uint8_t kcp_op_connect_ack = 2;
inline constexpr uint8_t kcp_op_disconnect = 3;
inline constexpr uint8_t kcp_op_heartbeat = 4;
inline constexpr uint8_t kcp_op_heartbeat_ack = 5;
inline constexpr uint8_t kcp_op_data = 6;

enum class kcp_code : uint8_t {
    connect = kcp_op_connect,
    connect_ack = kcp_op_connect_ack,
    disconnect = kcp_op_disconnect,
    heartbeat = kcp_op_heartbeat,
    heartbeat_ack = kcp_op_heartbeat_ack,
    data = kcp_op_data,
};

inline constexpr uint8_t kcp_magic1 = 0x62;
inline constexpr uint8_t kcp_magic2 = 0xf9;
inline constexpr uint8_t kcp_magic3 = 0x8e;

struct kcp_head {
    uint8_t magic1{kcp_magic1};
    uint8_t magic2{kcp_magic2};
    uint8_t magic3{kcp_magic3};
    kcp_code code{kcp_code::connect};
};

inline constexpr int32_t udp_mtu = 470;

inline constexpr int32_t kcp_head_size = sizeof(kcp_head);

inline constexpr int32_t kcp_mtu = udp_mtu - kcp_head_size;

inline constexpr int32_t kcp_recv_capacity = 1024;

inline auto make_kcp_ctrl(kcp_code code, uint32_t conv) {
    std::vector<uint8_t> msg;
    msg.resize(kcp_head_size + sizeof(conv));
    kcp_head head;
    head.code = code;
    memcpy(msg.data(), &head, sizeof(head));
    conv = htonl(conv);
    memcpy(msg.data() + sizeof(head), &conv, sizeof(conv));
    return msg;
}

inline auto make_kcp_data(const void* data, size_t len) {
    std::vector<uint8_t> msg;
    msg.resize(kcp_head_size + len);
    kcp_head head;
    head.code = kcp_code::data;
    memcpy(msg.data(), &head, sizeof(head));
    memcpy(msg.data() + sizeof(head), data, len);
    return msg;
}

inline void kcp_no_delay(IKCPCB* kcp, bool on) {
    if (on) {
        ikcp_nodelay(kcp, 1, 10, 2, 1);
        kcp->rx_minrto = 10;
    } else {
        ikcp_nodelay(kcp, 0, 40, 0, 0);
    }
}

inline IKCPCB* kcp_create_default(uint32_t conv, void* user) {
    auto* kcp = ikcp_create(conv, user);
    ikcp_wndsize(kcp, 256, 256);
    ikcp_setmtu(kcp, kcp_mtu);
    kcp_no_delay(kcp, true);
    return kcp;
}

}  // namespace simple
