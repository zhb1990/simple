#include <simple/log/types.h>

namespace simple {

std::string_view to_string_view(log_level level) noexcept {
    using namespace std::string_view_literals;
    switch (level) {
        case log_level::off:
            return "off"sv;
        case log_level::critical:
            return "critical"sv;
        case log_level::error:
            return "error"sv;
        case log_level::warn:
            return "warn"sv;
        case log_level::info:
            return "info"sv;
        case log_level::debug:
            return "debug"sv;
        case log_level::trace:
            return "trace"sv;
    }

    return ""sv;
}

std::string_view to_string_view_short(log_level level) noexcept {
    using namespace std::string_view_literals;
    switch (level) {
        case log_level::off:
            return "O"sv;
        case log_level::critical:
            return "C"sv;
        case log_level::error:
            return "E"sv;
        case log_level::warn:
            return "W"sv;
        case log_level::info:
            return "I"sv;
        case log_level::debug:
            return "D"sv;
        case log_level::trace:
            return "T"sv;
    }

    return ""sv;
}

log_level log_level_from_string_view(const std::string_view& strv) noexcept {
    if (strv.empty()) {
        return log_level::trace;
    }

    // 只判断下第一个字符，不需要完全判断
    switch (strv[0]) {
        case 'o':
        case 'O':
            return log_level::off;

        case 'c':
        case 'C':
            return log_level::critical;

        case 'e':
        case 'E':
            return log_level::error;

        case 'w':
        case 'W':
            return log_level::warn;

        case 'i':
        case 'I':
            return log_level::info;

        case 'd':
        case 'D':
            return log_level::debug;

        case 't':
        case 'T':
            [[fallthrough]];
        default:
            return log_level::trace;
    }
}

log_time_type log_time_from_string_view(const std::string_view& strv) noexcept {
    if (strv.empty()) {
        return log_time_type::local;
    }

    if (strv[0] == 'u' || strv[0] == 'U') {
        return log_time_type::utc;
    }

    return log_time_type::local;
}

}  // namespace simple
