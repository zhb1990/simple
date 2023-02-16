#pragma once
#include <simple/log/formatter.h>

#include <mutex>

namespace simple {

class log_appender {
  public:
    SIMPLE_API log_appender();

    virtual ~log_appender() noexcept = default;

    SIMPLE_NON_COPYABLE(log_appender)

    SIMPLE_API void log(const log_message& msg);

    SIMPLE_API void flush();

    SIMPLE_API void set_pattern(const std::string_view& pattern, log_time_type tp = log_time_type::local);

    SIMPLE_API void set_formatter(std::unique_ptr<log_formatter> formatter);

    SIMPLE_API void set_level(log_level level);

    [[nodiscard]] SIMPLE_API log_level level() const;

    [[nodiscard]] SIMPLE_API bool should_log(log_level level) const;

  protected:
    virtual void write(log_level level, const log_clock_point& point, const log_buf_t& buf, size_t color_start,
                       size_t color_stop) = 0;

    virtual void flush_unlock() = 0;

  private:
    atomic_log_level level_{log_level::trace};
    std::unique_ptr<log_formatter> formatter_;
    std::mutex mtx_;
};

}  // namespace simple
