#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace simple {

inline constexpr uint32_t socket_protocol_tcp = 0;
inline constexpr uint32_t socket_protocol_ssl = 1;
inline constexpr uint32_t socket_protocol_kcp = 2;
inline constexpr uint32_t socket_protocol_mask = 3;

enum class socket_protocol : uint32_t {
    tcp = socket_protocol_tcp,
    ssl = socket_protocol_ssl,
    kcp = socket_protocol_kcp,
};

inline constexpr uint32_t socket_class_server = 1;
inline constexpr uint32_t socket_class_session = 2;
inline constexpr uint32_t socket_class_client = 3;
inline constexpr uint32_t socket_class_mask = 3;

enum class socket_class : uint32_t {
    server = socket_class_server,
    session = socket_class_session,
    client = socket_class_client,
};

enum class socket_type : uint32_t {
    tcp_server = socket_protocol_tcp << 2 | socket_class_server,
    tcp_session = socket_protocol_tcp << 2 | socket_class_session,
    tcp_client = socket_protocol_tcp << 2 | socket_class_client,

    ssl_server = socket_protocol_ssl << 2 | socket_class_server,
    ssl_session = socket_protocol_ssl << 2 | socket_class_session,
    ssl_client = socket_protocol_ssl << 2 | socket_class_client,

    kcp_server = socket_protocol_kcp << 2 | socket_class_server,
    kcp_session = socket_protocol_kcp << 2 | socket_class_session,
    kcp_client = socket_protocol_kcp << 2 | socket_class_client,
};

constexpr std::string_view get_socket_type_strv(socket_type tp) {
    using namespace std::string_view_literals;
    switch (tp) {
        case socket_type::tcp_server:
            return "tcp_server"sv;
        case socket_type::tcp_session:
            return "tcp_session"sv;
        case socket_type::tcp_client:
            return "tcp_client"sv;
        case socket_type::ssl_server:
            return "ssl_server"sv;
        case socket_type::ssl_session:
            return "ssl_session"sv;
        case socket_type::ssl_client:
            return "ssl_client"sv;
        case socket_type::kcp_server:
            return "kcp_server"sv;
        case socket_type::kcp_session:
            return "kcp_session"sv;
        case socket_type::kcp_client:
            return "kcp_client"sv;
        default:  // NOLINT(clang-diagnostic-covered-switch-default)
            return "unknown"sv;
    }
}

inline constexpr uint32_t socket_type_mask = socket_protocol_mask << 2 | socket_class_mask;

constexpr socket_type get_socket_type(uint32_t id) { return static_cast<socket_type>(id & socket_type_mask); }

constexpr socket_protocol get_socket_protocol(uint32_t id) {
    return static_cast<socket_protocol>(id >> 2 & socket_protocol_mask);
}

constexpr socket_class get_socket_class(uint32_t id) { return static_cast<socket_class>(id & socket_class_mask); }

struct socket_trace {
    int64_t read{0};
    int64_t write{0};
    int64_t read_time{0};
    int64_t write_time{0};
    int64_t write_queue{0};
};

struct socket_stat : socket_trace {
    uint32_t id{0};
    socket_type type{socket_type::tcp_server};
    std::string local;
    std::string remote;
};

}  // namespace simple
