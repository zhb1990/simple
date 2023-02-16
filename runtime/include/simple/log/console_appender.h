#pragma once
#include <simple/log/appender.h>

namespace simple {

class console_appender : public log_appender {
  public:
    DS_API explicit console_appender(FILE *target_file);

    ~console_appender() noexcept override = default;

    DS_NON_COPYABLE(console_appender)

  protected:
    DS_API void write(log_level level, const log_clock_point &, const log_buf_t &buf, size_t color_start,
                      size_t color_stop) override;

    DS_API void flush_unlock() override;

  private:
    void write_range(const log_buf_t &formatted, size_t start, size_t end);
    void write_strv(const std::string_view &strv);

    FILE *file_;
    bool enable_color_;
};

class stdout_appender final : public console_appender {
  public:
    DS_API stdout_appender();
};

class stderr_appender final : public console_appender {
  public:
    DS_API stderr_appender();
};

}  // namespace simple
