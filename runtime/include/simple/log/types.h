#pragma once

#include <fmt/format.h>
#include <simple/config.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace simple {

enum class log_level : uint8_t { off = 0, critical = 1, error = 2, warn = 3, info = 4, debug = 5, trace = 6 };

enum class log_time_type { local = 0, utc = 1 };

using atomic_log_level = std::atomic<log_level>;

using log_levels = std::unordered_map<std::string, log_level>;

using log_clock = std::chrono::system_clock;

using log_clock_point = std::chrono::system_clock::time_point;

using log_buf_t = fmt::basic_memory_buffer<char, 250>;

struct log_message;

DS_API std::string_view to_string_view(log_level level) noexcept;

DS_API std::string_view to_string_view_short(log_level level) noexcept;

DS_API log_level log_level_from_string_view(const std::string_view& strv) noexcept;

DS_API log_time_type log_time_from_string_view(const std::string_view& strv) noexcept;

class log_formatter;

class log_appender;

using log_appender_ptr = std::shared_ptr<log_appender>;

class logger;

using logger_ptr = std::shared_ptr<logger>;

using const_logger_ptr = std::shared_ptr<const logger>;

}  // namespace simple
