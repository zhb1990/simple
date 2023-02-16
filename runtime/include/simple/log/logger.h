#pragma once
#include <simple/config.h>
#include <simple/log/formatter.h>
#include <simple/log/types.h>

#include <ranges>
#include <source_location>
#include <string>

namespace simple {

class logger : public std::enable_shared_from_this<logger> {
  public:
    // Logger with range on appenders
    template <std::ranges::range Range>
    logger(const std::string_view &name, const Range &range, bool async = false)
        : name_(name), async_(async), appenders_(std::ranges::begin(range), std::ranges::end(range)) {}

    // Logger with single appender
    DS_API logger(const std::string_view &name, log_appender_ptr appender, bool async = false);

    DS_NON_COPYABLE(logger)

    ~logger() noexcept = default;

    DS_API void set_level(log_level level);

    [[nodiscard]] DS_API log_level level() const { return level_; }

    [[nodiscard]] DS_API bool should_log(log_level level) const;

    [[nodiscard]] DS_API const std::string &name() const { return name_; }

    DS_API void set_formatter(std::unique_ptr<log_formatter> formatter);

    DS_API void set_pattern(const std::string_view &pattern, log_time_type time_type = log_time_type::local);

    DS_API void flush() const;

    DS_API void log(const std::source_location &source, log_level level, log_buf_t &buf) const;

    template <typename... Args>
    void log(const std::source_location &source, log_level level, const fmt::format_string<Args...> &fmt,
             Args &&...args) const {
        if (!should_log(level)) return;

        try {
            log_buf_t buf;
            fmt::detail::vformat_to(buf, fmt::string_view(fmt), fmt::make_format_args(std::forward<Args>(args)...));
            log(source, level, buf);
        } catch (...) {
        }
    }

    template <typename... Args>
    void critical(const std::source_location &source, const fmt::format_string<Args...> &fmt, Args &&...args) const {
        return log(source, log_level::critical, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const std::source_location &source, const fmt::format_string<Args...> &fmt, Args &&...args) const {
        return log(source, log_level::error, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(const std::source_location &source, const fmt::format_string<Args...> &fmt, Args &&...args) const {
        return log(source, log_level::warn, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(const std::source_location &source, const fmt::format_string<Args...> &fmt, Args &&...args) const {
        return log(source, log_level::info, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(const std::source_location &source, const fmt::format_string<Args...> &fmt, Args &&...args) const {
        return log(source, log_level::debug, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace(const std::source_location &source, const fmt::format_string<Args...> &fmt, Args &&...args) const {
        return log(source, log_level::trace, fmt, std::forward<Args>(args)...);
    }

    template <typename T>
    void critical(const T &t, const std::source_location &source = std::source_location::current()) const {
        return log(source, log_level::critical, "{}", t);
    }

    template <typename T>
    void error(const T &t, const std::source_location &source = std::source_location::current()) const {
        return log(source, log_level::error, "{}", t);
    }

    template <typename T>
    void warn(const T &t, const std::source_location &source = std::source_location::current()) const {
        return log(source, log_level::warn, "{}", t);
    }

    template <typename T>
    void info(const T &t, const std::source_location &source = std::source_location::current()) const {
        return log(source, log_level::info, "{}", t);
    }

    template <typename T>
    void debug(const T &t, const std::source_location &source = std::source_location::current()) const {
        return log(source, log_level::debug, "{}", t);
    }

    template <typename T>
    void trace(const T &t, const std::source_location &source = std::source_location::current()) const {
        return log(source, log_level::trace, "{}", t);
    }

  private:
    friend class log_system;

    void backend_log(const log_message &msg) const;

    void backend_flush() const;

    std::string name_;
    bool async_;
    atomic_log_level level_{log_level::trace};
    std::vector<log_appender_ptr> appenders_;
};

}  // namespace simple
