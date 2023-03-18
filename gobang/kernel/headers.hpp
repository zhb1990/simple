#pragma once
#include <cstdint>

constexpr uint32_t flag_valid = 0xe4;

struct net_header {
    uint32_t flag : 8;
    uint32_t len : 24;
    // 消息id
    uint16_t id;
    uint16_t padding;
    // 消息会话id
    uint64_t session;

    [[nodiscard]] bool valid() const noexcept { return flag == flag_valid; }
};

struct ws_header {
    uint8_t flag;
    uint8_t padding[5];
    uint16_t id;
    uint64_t session;

    [[nodiscard]] bool valid() const noexcept { return flag == flag_valid; }
};

struct shm_header {
    // 来源server id
    uint16_t from;
    // 目标server id
    uint16_t to;
    // 消息id
    uint16_t id;
    // 字节对齐
    uint16_t padding;
    // 消息会话id
    uint64_t session;
};



