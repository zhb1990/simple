#pragma once
#include <simple/log/formatter.h>

#include <mutex>

namespace simple {

class log_appender {
  public:
    DS_API log_appender();

    virtual ~log_appender() noexcept = default;

    DS_NON_COPYABLE(log_appender)

    DS_API void log(const log_message& msg);

    DS_API void flush();

    DS_API void set_pattern(const std::string_view& pattern, log_time_type tp = log_time_type::local);

    DS_API void set_formatter(std::unique_ptr<log_formatter> formatter);

    DS_API void set_level(log_level level);

    [[nodiscard]] DS_API log_level level() const;

    [[nodiscard]] DS_API bool should_log(log_level level) const;

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
