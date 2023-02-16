#pragma once
#include <simple/log/logger.h>

#include <cstdio>
#include <source_location>

/*
 * 日志库改自 https://github.com/gabime/spdlog
 */

namespace simple {

SIMPLE_API void write_console(const std::string_view &strv, FILE *file);

template <typename... Args>
void print_error(fmt::format_string<Args...> fmt, Args &&...args) {
    try {
        log_buf_t buf;
        fmt::detail::vformat_to(buf, fmt::string_view(fmt), fmt::make_format_args(std::forward<Args>(args)...));
        write_console({buf.data(), buf.size()}, stderr);
    } catch (...) {
    }
}

SIMPLE_API void load_log_config(const std::string &utf8_path);

SIMPLE_API void register_logger(logger_ptr new_logger);

SIMPLE_API void initialize_logger(logger_ptr new_logger);

SIMPLE_API logger_ptr find_logger(const std::string &name);

SIMPLE_API void drop_logger(const std::string &name);

SIMPLE_API void drop_all_loggers();

SIMPLE_API logger_ptr default_logger();

SIMPLE_API void set_default_logger(logger_ptr new_default_logger);

SIMPLE_API void set_log_level(log_level level);

SIMPLE_API void set_log_levels(log_levels levels, const log_level *global_level = nullptr);

SIMPLE_API void set_log_formatter(std::unique_ptr<log_formatter> formatter);

SIMPLE_API void set_log_pattern(const std::string_view &pattern, log_time_type time_type = log_time_type::local);

SIMPLE_API void log_flush();

SIMPLE_API void set_log_flush_interval(int64_t seconds);

template <typename... Args>
struct log {
    log(log_level level, const fmt::format_string<Args...> &fmt, Args &&...args,
        const std::source_location &source = std::source_location::current()) {
        default_logger()->log(source, level, fmt, std::forward<Args>(args)...);
    }

    log(const logger_ptr &ptr, log_level level, const fmt::format_string<Args...> &fmt, Args &&...args,
        const std::source_location &source = std::source_location::current()) {
        ptr->log(source, level, fmt, std::forward<Args>(args)...);
    }
};

template <typename... Args>
log(log_level level, const fmt::format_string<Args...> &fmt, Args &&...args) -> log<Args...>;

template <typename... Args>
log(const logger_ptr &ptr, log_level level, const fmt::format_string<Args...> &fmt, Args &&...args) -> log<Args...>;

template <typename... Args>
struct critical {
    explicit critical(const fmt::format_string<Args...> &fmt, Args &&...args,
                      const std::source_location &source = std::source_location::current()) {
        default_logger()->log(source, log_level::critical, fmt, std::forward<Args>(args)...);
    }

    critical(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args,
             const std::source_location &source = std::source_location::current()) {
        ptr->log(source, log_level::critical, fmt, std::forward<Args>(args)...);
    }
};

template <typename... Args>
critical(const fmt::format_string<Args...> &fmt, Args &&...args) -> critical<Args...>;

template <typename... Args>
critical(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args) -> critical<Args...>;

template <typename... Args>
struct error {
    explicit error(const fmt::format_string<Args...> &fmt, Args &&...args,
                   const std::source_location &source = std::source_location::current()) {
        default_logger()->log(source, log_level::error, fmt, std::forward<Args>(args)...);
    }

    error(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args,
          const std::source_location &source = std::source_location::current()) {
        ptr->log(source, log_level::error, fmt, std::forward<Args>(args)...);
    }
};

template <typename... Args>
error(const fmt::format_string<Args...> &fmt, Args &&...args) -> error<Args...>;

template <typename... Args>
error(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args) -> error<Args...>;

template <typename... Args>
struct warn {
    explicit warn(const fmt::format_string<Args...> &fmt, Args &&...args,
                  const std::source_location &source = std::source_location::current()) {
        default_logger()->log(source, log_level::warn, fmt, std::forward<Args>(args)...);
    }

    warn(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args,
         const std::source_location &source = std::source_location::current()) {
        ptr->log(source, log_level::warn, fmt, std::forward<Args>(args)...);
    }
};

template <typename... Args>
warn(const fmt::format_string<Args...> &fmt, Args &&...args) -> warn<Args...>;

template <typename... Args>
warn(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args) -> warn<Args...>;

template <typename... Args>
struct info {
    explicit info(const fmt::format_string<Args...> &fmt, Args &&...args,
                  const std::source_location &source = std::source_location::current()) {
        default_logger()->log(source, log_level::info, fmt, std::forward<Args>(args)...);
    }

    info(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args,
         const std::source_location &source = std::source_location::current()) {
        ptr->log(source, log_level::info, fmt, std::forward<Args>(args)...);
    }
};

template <typename... Args>
info(const fmt::format_string<Args...> &fmt, Args &&...args) -> info<Args...>;

template <typename... Args>
info(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args) -> info<Args...>;

template <typename... Args>
struct debug {
    explicit debug(const fmt::format_string<Args...> &fmt, Args &&...args,
                   const std::source_location &source = std::source_location::current()) {
        default_logger()->log(source, log_level::debug, fmt, std::forward<Args>(args)...);
    }

    debug(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args,
          const std::source_location &source = std::source_location::current()) {
        ptr->log(source, log_level::debug, fmt, std::forward<Args>(args)...);
    }
};

template <typename... Args>
debug(const fmt::format_string<Args...> &fmt, Args &&...args) -> debug<Args...>;

template <typename... Args>
debug(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args) -> debug<Args...>;

template <typename... Args>
struct trace {
    explicit trace(const fmt::format_string<Args...> &fmt, Args &&...args,
                   const std::source_location &source = std::source_location::current()) {
        default_logger()->log(source, log_level::trace, fmt, std::forward<Args>(args)...);
    }

    trace(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args,
          const std::source_location &source = std::source_location::current()) {
        ptr->log(source, log_level::trace, fmt, std::forward<Args>(args)...);
    }
};

template <typename... Args>
trace(const fmt::format_string<Args...> &fmt, Args &&...args) -> trace<Args...>;

template <typename... Args>
trace(const logger_ptr &ptr, const fmt::format_string<Args...> &fmt, Args &&...args) -> trace<Args...>;

}  // namespace simple
