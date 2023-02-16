#include <simple/log/formatter.h>

#include <simple/log/default_flags.hpp>

namespace simple {
constexpr std::string_view default_eol = "\n";

log_formatter::log_formatter() : log_formatter(log_time_type::local) {}

log_formatter::log_formatter(log_time_type tp) : time_type_(tp), need_localtime_(true) {
    formatters_.emplace_back(std::make_unique<default_flag>());
}

log_formatter::log_formatter(const std::string_view& pattern) : log_formatter(pattern, log_time_type::local) {}

log_formatter::log_formatter(const std::string_view& pattern, log_time_type tp) : time_type_(tp), need_localtime_(false) {
    set_pattern(pattern);
}

static log_padding handle_padding(std::string_view::const_iterator& it, const std::string_view::const_iterator& end) {
    if (it == end) {
        return {};
    }

    log_padding::pad_side side;
    switch (*it) {
        case '-':
            side = log_padding::pad_side::right;
            ++it;
            break;
        case '=':
            side = log_padding::pad_side::center;
            ++it;
            break;
        default:
            side = log_padding::pad_side::left;
            break;
    }

    if (it == end || !std::isdigit(static_cast<unsigned char>(*it))) {
        return {};
    }

    auto width = static_cast<size_t>(*it) - '0';
    for (++it; it != end && std::isdigit(static_cast<unsigned char>(*it)); ++it) {
        const auto digit = static_cast<size_t>(*it) - '0';
        width = width * 10 + digit;
    }

    bool truncate;
    if (it != end && *it == '!') {
        truncate = true;
        ++it;
    } else {
        truncate = false;
    }
    return {(std::min)(width, padding_spaces.size()), side, truncate, true};
}

template <scoped_padder_type Padder>
void log_formatter::handle_flag(char flag, log_padding padding) {
    // 自定义flag
    if (const auto it = custom_flags_.find(flag); it != custom_flags_.end()) {
        formatters_.emplace_back(it->second(padding));
        return;
    }

    // 默认的flag
    switch (flag) {
        case 'n':  // logger 名字
            formatters_.emplace_back(std::make_unique<name_flag<Padder>>(padding));
            break;

        case 'l':  // level
            formatters_.emplace_back(std::make_unique<level_flag<Padder>>(padding));
            break;

        case 'L':  // short level
            formatters_.emplace_back(std::make_unique<short_level_flag<Padder>>(padding));
            break;

        case 't':  // thread id
            formatters_.emplace_back(std::make_unique<tid_flag<Padder>>(padding));
            break;

        case 'v':  // the message text
            formatters_.emplace_back(std::make_unique<log_value_flag<Padder>>(padding));
            break;

        case 'a':  // short weekday
            formatters_.emplace_back(std::make_unique<short_weekday_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'A':  // weekday
            formatters_.emplace_back(std::make_unique<weekday_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'b':
        case 'h':  // short month
            formatters_.emplace_back(std::make_unique<short_month_name_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'B':  // month
            formatters_.emplace_back(std::make_unique<month_name_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'c':  // datetime
            formatters_.emplace_back(std::make_unique<date_time_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'C':  // year 2 digits
            formatters_.emplace_back(std::make_unique<short_year_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'Y':  // year 4 digits
            formatters_.emplace_back(std::make_unique<year_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'D':
        case 'x':  // datetime MM/DD/YY
            formatters_.emplace_back(std::make_unique<short_date_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'm':  // month 1-12
            formatters_.emplace_back(std::make_unique<month_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'd':  // day of month 1-31
            formatters_.emplace_back(std::make_unique<day_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'H':  // hours 24
            formatters_.emplace_back(std::make_unique<hour24_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'I':  // hours 12
            formatters_.emplace_back(std::make_unique<hour12_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'M':  // minutes
            formatters_.emplace_back(std::make_unique<min_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'S':  // seconds
            formatters_.emplace_back(std::make_unique<second_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'e':  // milliseconds
            formatters_.emplace_back(std::make_unique<millis_flag<Padder>>(padding));
            break;

        case 'f':  // microseconds
            formatters_.emplace_back(std::make_unique<micros_flag<Padder>>(padding));
            break;

        case 'F':  // nanoseconds
            formatters_.emplace_back(std::make_unique<nanosecond_flag<Padder>>(padding));
            break;

        case 'E':  // seconds since epoch
            formatters_.emplace_back(std::make_unique<timestamp_flag<Padder>>(padding));
            break;

        case 'p':  // am/pm
            formatters_.emplace_back(std::make_unique<noon_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'r':  // 12 hour clock 02:55:02 pm
            formatters_.emplace_back(std::make_unique<clock_hour12_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'R':  // 24-hour HH:MM time
            formatters_.emplace_back(std::make_unique<short_clock_hour24_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'T':
        case 'X':  // ISO 8601 time format (HH:MM:SS)
            formatters_.emplace_back(std::make_unique<clock_hour24_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'z':  // timezone
            formatters_.emplace_back(std::make_unique<timezone_flag<Padder>>(padding));
            need_localtime_ = true;
            break;

        case 'P':  // pid
            formatters_.emplace_back(std::make_unique<pid_flag<Padder>>(padding));
            break;

        case '^':  // color range start
            formatters_.emplace_back(std::make_unique<color_start_flag>(padding));
            break;

        case '$':  // color range end
            formatters_.emplace_back(std::make_unique<color_stop_flag>(padding));
            break;

        case '@':  // source location (filename:line)
            formatters_.emplace_back(std::make_unique<source_location_flag<Padder>>(padding));
            break;

        case 's':  // short source filename - without directory name
            formatters_.emplace_back(std::make_unique<short_filename_flag<Padder>>(padding));
            break;

        case 'g':  // full source filename
            formatters_.emplace_back(std::make_unique<filename_flag<Padder>>(padding));
            break;

        case '#':  // source line number
            formatters_.emplace_back(std::make_unique<line_flag<Padder>>(padding));
            break;

        case '!':  // source func name
            formatters_.emplace_back(std::make_unique<func_flag<Padder>>(padding));
            break;

        case '%':  // % char
            formatters_.emplace_back(std::make_unique<ch_flag>('%'));
            break;

        case 'u':  // elapsed time since last log message in nanoseconds
            formatters_.emplace_back(std::make_unique<elapsed_time_flag<Padder, std::chrono::nanoseconds>>(padding));
            break;

        case 'i':  // elapsed time since last log message in micros
            formatters_.emplace_back(std::make_unique<elapsed_time_flag<Padder, std::chrono::microseconds>>(padding));
            break;

        case 'o':  // elapsed time since last log message in millis
            formatters_.emplace_back(std::make_unique<elapsed_time_flag<Padder, std::chrono::milliseconds>>(padding));
            break;

        case 'O':  // elapsed time since last log message in seconds
            formatters_.emplace_back(std::make_unique<elapsed_time_flag<Padder, std::chrono::seconds>>(padding));
            break;

        default:  // Unknown flag appears as is
            auto unknown_flag = std::make_unique<aggregate_flag>();
            if (!padding.truncate) {
                unknown_flag->add_ch('%');
                unknown_flag->add_ch(flag);
                formatters_.emplace_back(std::move(unknown_flag));
            } else {
                padding.truncate = false;
                formatters_.emplace_back(std::make_unique<func_flag<Padder>>(padding));
                unknown_flag->add_ch(flag);
                formatters_.emplace_back(std::move(unknown_flag));
            }
            break;
    }
}

void log_formatter::set_pattern(const std::string_view& pattern) {
    const auto end = pattern.end();
    const auto user_chars = std::make_unique<aggregate_flag>();
    formatters_.clear();
    need_localtime_ = false;
    for (auto it = pattern.begin(); it != end; ++it) {
        if (*it == '%') {
            if (!user_chars->empty()) {
                formatters_.emplace_back(user_chars->clone());
                user_chars->clear();
            }

            const auto padding = handle_padding(++it, end);
            if (it != end) {
                if (padding.enabled) {
                    handle_flag<scoped_padder>(*it, padding);
                } else {
                    handle_flag<null_scoped_padder>(*it, padding);
                }
            } else {
                break;
            }
        } else {
            user_chars->add_ch(*it);
        }
    }
}

void log_formatter::format(const log_message& msg, log_buf_t& dest) {
    if (need_localtime_) {
        const auto secs = std::chrono::duration_cast<std::chrono::seconds>(msg.point.time_since_epoch());
        if (secs != last_log_secs_) {
            if (time_type_ == log_time_type::local) {
                cached_tm_ = fmt::localtime(msg.point);
            } else {
                cached_tm_ = fmt::gmtime(msg.point);
            }

            last_log_secs_ = secs;
        }
    }

    for (const auto& f : formatters_) {
        f->format(msg, cached_tm_, dest);
    }

    dest.append(default_eol);
}

std::unique_ptr<log_formatter> log_formatter::clone() const {
    auto cloned = std::make_unique<log_formatter>("", time_type_);
    cloned->need_localtime_ = need_localtime_;
    cloned->custom_flags_ = custom_flags_;
    cloned->formatters_.reserve(formatters_.size());
    for (const auto& flag : formatters_) {
        cloned->formatters_.emplace_back(flag->clone());
    }
    return cloned;
}

log_formatter& log_formatter::add_custom_flag(char ch, create_flag_func_t func) {
    custom_flags_[ch] = std::move(func);
    return *this;
}

}  // namespace simple
