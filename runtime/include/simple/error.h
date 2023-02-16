#pragma once
#include <simple/config.h>

#include <system_error>

namespace simple {

enum class socket_errors {
    // kcp 校验失败
    kcp_check_failed,
    // kcp 心跳超时
    kcp_heartbeat_timeout,
    // kcp 协议出错
    kcp_protocol_error,
    // 主动断开
    initiative_disconnect,
};

enum class coro_errors {
    canceled,
    broken_promise,
    // 错误的行为
    invalid_action
};

SIMPLE_API const std::error_category& get_socket_category();

SIMPLE_API const std::error_category& get_coro_category();

}  // namespace simple

template <>
struct std::is_error_code_enum<simple::socket_errors> {
    static constexpr bool value = true;
};

template <>
struct std::is_error_code_enum<simple::coro_errors> {
    static constexpr bool value = true;
};

namespace simple {

inline std::error_code make_error_code(socket_errors e) { return {static_cast<int>(e), get_socket_category()}; }

inline std::error_code make_error_code(coro_errors e) { return {static_cast<int>(e), get_coro_category()}; }

}  // namespace simple
