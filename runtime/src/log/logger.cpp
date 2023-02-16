#include <simple/log/appender.h>
#include <simple/log/log_system.h>
#include <simple/log/logger.h>
#include <simple/log/logmsg.h>
#include <simple/utils/os.h>

namespace simple {

logger::logger(const std::string_view& name, log_appender_ptr appender, bool async)
    : name_(name), async_(async), appenders_({std::move(appender)}) {}

void logger::set_level(log_level level) { level_.store(level, std::memory_order::relaxed); }

bool logger::should_log(log_level level) const {
    return static_cast<uint8_t>(level) <= static_cast<uint8_t>(level_.load(std::memory_order::relaxed));
}

void logger::set_formatter(std::unique_ptr<log_formatter> formatter) {
    for (auto it = appenders_.begin(); it != appenders_.end(); ++it) {
        if (std::next(it) == appenders_.end()) {
            (*it)->set_formatter(std::move(formatter));
            break;
        }
        (*it)->set_formatter(formatter->clone());
    }
}

void logger::set_pattern(const std::string_view& pattern, log_time_type time_type) {
    auto new_formatter = std::make_unique<log_formatter>(pattern, time_type);
    set_formatter(std::move(new_formatter));
}

void logger::flush() const {
    if (!async_) {
        return backend_flush();
    }

    auto& system = log_system::instance();
    auto* msg = system.new_message();
    msg->tp = log_message::flush;
    msg->ptr = shared_from_this();
    system.send(msg);
}

void logger::log(const std::source_location& source, log_level level, log_buf_t& buf) const {
    if (!should_log(level)) {
        return;
    }

    auto fill_msg = [&](log_message& msg) {
        msg.tp = log_message::log;
        msg.ptr = shared_from_this();
        msg.level = level;
        msg.point = std::chrono::system_clock::now();
        msg.tid = static_cast<int32_t>(os::tid());
        msg.source = source;
        msg.buf = std::move(buf);
    };

    if (!async_) {
        log_message msg;
        fill_msg(msg);
        return backend_log(msg);
    }

    auto& system = log_system::instance();
    auto* msg = system.new_message();
    fill_msg(*msg);

    system.send(msg);
}

void logger::backend_log(const log_message& msg) const {
    for (const auto& appender : appenders_) {
        try {
            appender->log(msg);
        } catch (...) {
        }
    }
}

void logger::backend_flush() const {
    for (const auto& appender : appenders_) {
        try {
            appender->flush();
        } catch (...) {
        }
    }
}

}  // namespace simple
