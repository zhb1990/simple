#include <simple/log/appender.h>
#include <simple/log/logmsg.h>

namespace simple {

log_appender::log_appender() { formatter_ = std::make_unique<log_formatter>(); }

// ReSharper disable once CppMemberFunctionMayBeConst
void log_appender::log(const log_message& msg) {
    if (!should_log(msg.level)) return;

    std::scoped_lock lock(mtx_);
    log_buf_t buf;
    formatter_->format(msg, buf);
    return write(msg.level, msg.point, buf, msg.color_start, msg.color_stop);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void log_appender::flush() {
    std::scoped_lock lock(mtx_);
    flush_unlock();
}

void log_appender::set_pattern(const std::string_view& pattern, log_time_type tp) {
    std::scoped_lock lock(mtx_);
    formatter_ = std::make_unique<log_formatter>(pattern, tp);
}

void log_appender::set_formatter(std::unique_ptr<log_formatter> formatter) {
    std::scoped_lock lock(mtx_);
    formatter_ = std::move(formatter);
}

void log_appender::set_level(log_level level) { level_.store(level, std::memory_order::relaxed); }

log_level log_appender::level() const { return level_.load(std::memory_order::relaxed); }

bool log_appender::should_log(log_level level) const {
    return static_cast<uint8_t>(level) <= static_cast<uint8_t>(level_.load(std::memory_order::relaxed));
}

}  // namespace simple
