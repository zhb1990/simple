#pragma once

#if defined(_WIN32)

#include <simple/log/appender.h>

namespace simple {

class msvc_appender : public log_appender {
  public:
    msvc_appender() = default;

    ~msvc_appender() noexcept override = default;

    SIMPLE_NON_COPYABLE(msvc_appender)

  protected:
    SIMPLE_API void write(log_level, const log_clock_point&, const log_buf_t& buf, size_t, size_t) override;

    SIMPLE_API void flush_unlock() override;
};

}  // namespace simple

#endif
