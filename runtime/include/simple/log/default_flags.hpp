#pragma once
#include <fmt/chrono.h>
#include <simple/log/flag.h>
#include <simple/log/logger.h>
#include <simple/log/logmsg.h>
#include <simple/utils/os.h>
#include <simple/utils/time.h>

#include <array>

namespace simple {

inline constexpr std::string_view padding_spaces = "                                                                ";

class scoped_padder {  // NOLINT(cppcoreguidelines-special-member-functions)
  public:
    scoped_padder(size_t wrapped_size, const log_padding &info, log_buf_t &dest);

    ~scoped_padder();

    static int count_digits(uint64_t num) { return fmt::detail::count_digits(num); }

  private:
    const log_padding &info_;
    log_buf_t &dest_;
    int32_t remaining_pad_;
};

inline scoped_padder::scoped_padder(size_t wrapped_size, const log_padding &info, log_buf_t &dest) : info_(info), dest_(dest) {
    remaining_pad_ = static_cast<int32_t>(info.width - wrapped_size);
    if (remaining_pad_ <= 0) {
        return;
    }

    if (info_.side == log_padding::pad_side::left) {
        dest_.append(padding_spaces.substr(0, remaining_pad_));
        remaining_pad_ = 0;
    } else if (info_.side == log_padding::pad_side::center) {
        const auto half_pad = remaining_pad_ / 2;
        const auto reminder = remaining_pad_ & 1;
        dest_.append(padding_spaces.substr(0, half_pad));
        remaining_pad_ = half_pad + reminder;  // for the right side
    }
}

inline scoped_padder::~scoped_padder() {
    if (remaining_pad_ >= 0) {
        dest_.append(padding_spaces.substr(0, remaining_pad_));
    } else if (info_.truncate) {
        const auto new_size = static_cast<int32_t>(dest_.size()) + remaining_pad_;
        dest_.resize(static_cast<size_t>(new_size));
    }
}

struct null_scoped_padder {
    null_scoped_padder(size_t, const log_padding &, log_buf_t &) {}

    static int count_digits(uint64_t) { return 0; }
};

// log 名字flag "%n"
template <scoped_padder_type ScopedPadder>
class name_flag final : public log_flag {
  public:
    explicit name_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(name_flag)

    ~name_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<name_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        if (msg.ptr) {
            const auto &name = msg.ptr->name();
            ScopedPadder p(name.size(), pad_, dest);
            dest.append(name);
        }
    }
};

// log 等级flag "%l"
template <scoped_padder_type ScopedPadder>
class level_flag final : public log_flag {
  public:
    explicit level_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(level_flag)

    ~level_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<level_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto strv = to_string_view(msg.level);
        ScopedPadder p(strv.size(), pad_, dest);
        dest.append(strv);
    }
};

// log 等级缩写flag "%L"
template <scoped_padder_type ScopedPadder>
class short_level_flag final : public log_flag {
  public:
    explicit short_level_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(short_level_flag)

    ~short_level_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<short_level_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto strv = to_string_view_short(msg.level);
        ScopedPadder p(strv.size(), pad_, dest);
        dest.append(strv);
    }
};

namespace detail {
inline std::string_view am_or_pm(const tm &t) { return t.tm_hour >= 12 ? "PM" : "AM"; }

inline int32_t to_12_hour(const tm &t) { return t.tm_hour > 12 ? t.tm_hour - 12 : t.tm_hour; }

inline constexpr std::array<std::string_view, 7> short_weekdays{{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}};

inline constexpr std::array<std::string_view, 7> weekdays{
    {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}};

inline constexpr std::array<std::string_view, 12> short_months{
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sept", "Oct", "Nov", "Dec"}};

inline constexpr std::array<std::string_view, 12> months{
    {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"}};

inline void pad2(int n, log_buf_t &dest) {
    if (n >= 0 && n < 100) {  // 0-99
        dest.push_back(static_cast<char>('0' + n / 10));
        dest.push_back(static_cast<char>('0' + n % 10));
    } else {
        // unlikely, but just in case, let fmt deal with it
        fmt::detail::vformat_to(dest, fmt::string_view{"{:02}"}, fmt::make_format_args(n));
    }
}

inline void pad3(int n, log_buf_t &dest) {
    if (n < 1000) {
        dest.push_back(static_cast<char>(n / 100 + '0'));
        n = n % 100;
        dest.push_back(static_cast<char>(n / 10 + '0'));
        dest.push_back(static_cast<char>(n % 10 + '0'));
    } else {
        dest.append(fmt::format_int(n));
    }
}

inline void pad_int(uint64_t n, int32_t width, log_buf_t &dest) {
    for (auto digits = fmt::detail::count_digits(n); digits < width; ++digits) {
        dest.push_back('0');
    }
    dest.append(fmt::format_int(n));
}

}  // namespace detail

// 星期缩写 flag "%a"
template <scoped_padder_type ScopedPadder>
class short_weekday_flag final : public log_flag {
  public:
    explicit short_weekday_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(short_weekday_flag)

    ~short_weekday_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<short_weekday_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto &strv = detail::short_weekdays[tm_time.tm_wday];
        ScopedPadder p(strv.size(), pad_, dest);
        dest.append(strv);
    }
};

// 星期 flag "%A"
template <scoped_padder_type ScopedPadder>
class weekday_flag final : public log_flag {
  public:
    explicit weekday_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(weekday_flag)

    ~weekday_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<weekday_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto &strv = detail::weekdays[tm_time.tm_wday];
        ScopedPadder p(strv.size(), pad_, dest);
        dest.append(strv);
    }
};

// 月份缩写 flag "%b" or "%h"
template <scoped_padder_type ScopedPadder>
class short_month_name_flag final : public log_flag {
  public:
    explicit short_month_name_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(short_month_name_flag)

    ~short_month_name_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<short_month_name_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto &strv = detail::short_months[tm_time.tm_mon];
        ScopedPadder p(strv.size(), pad_, dest);
        dest.append(strv);
    }
};

// 月份 flag "%B"
template <scoped_padder_type ScopedPadder>
class month_name_flag final : public log_flag {
  public:
    explicit month_name_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(month_name_flag)

    ~month_name_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<month_name_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto &strv = detail::months[tm_time.tm_mon];
        ScopedPadder p(strv.size(), pad_, dest);
        dest.append(strv);
    }
};

// 日期(Fri Aug 26 20:07:45 2022)  flag "%c"
template <scoped_padder_type ScopedPadder>
class date_time_flag final : public log_flag {
  public:
    explicit date_time_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(date_time_flag)

    ~date_time_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<date_time_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(24, pad_, dest);
        dest.append(detail::short_weekdays[tm_time.tm_wday]);
        dest.push_back(' ');
        dest.append(detail::short_months[tm_time.tm_mon]);
        dest.push_back(' ');
        dest.append(fmt::format_int(tm_time.tm_mday));
        dest.push_back(' ');
        detail::pad2(tm_time.tm_hour, dest);
        dest.push_back(':');
        detail::pad2(tm_time.tm_min, dest);
        dest.push_back(':');
        detail::pad2(tm_time.tm_sec, dest);
        dest.push_back(' ');
        dest.append(fmt::format_int(tm_time.tm_year + 1900));
    }
};

// 年2位数 flag "%C"
template <scoped_padder_type ScopedPadder>
class short_year_flag final : public log_flag {
  public:
    explicit short_year_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(short_year_flag)

    ~short_year_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<short_year_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(2, pad_, dest);
        detail::pad2(tm_time.tm_year % 100, dest);
    }
};

// 年 4位数 flag "%Y"
template <scoped_padder_type ScopedPadder>
class year_flag final : public log_flag {
  public:
    explicit year_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(year_flag)

    ~year_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<year_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(4, pad_, dest);
        dest.append(fmt::format_int(tm_time.tm_year + 1900));
    }
};

// 月2位数 flag "%m"
template <scoped_padder_type ScopedPadder>
class month_flag final : public log_flag {
  public:
    explicit month_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(month_flag)

    ~month_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<month_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(2, pad_, dest);
        detail::pad2(tm_time.tm_mon + 1, dest);
    }
};

// 天2位数 flag "%d"
template <scoped_padder_type ScopedPadder>
class day_flag final : public log_flag {
  public:
    explicit day_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(day_flag)

    ~day_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<day_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(2, pad_, dest);
        detail::pad2(tm_time.tm_mday, dest);
    }
};

// 短日期(08/23/01) flag "%D" or "%x"
template <scoped_padder_type ScopedPadder>
class short_date_flag final : public log_flag {
  public:
    explicit short_date_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(short_date_flag)

    ~short_date_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<short_date_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(10, pad_, dest);
        detail::pad2(tm_time.tm_mon + 1, dest);
        dest.push_back('/');
        detail::pad2(tm_time.tm_mday, dest);
        dest.push_back('/');
        detail::pad2(tm_time.tm_year % 100, dest);
    }
};

// 小时 24小时制 flag "%H"
template <scoped_padder_type ScopedPadder>
class hour24_flag final : public log_flag {
  public:
    explicit hour24_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(hour24_flag)

    ~hour24_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<hour24_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(2, pad_, dest);
        detail::pad2(tm_time.tm_hour, dest);
    }
};

// 小时 12小时制 flag "%I"
template <scoped_padder_type ScopedPadder>
class hour12_flag final : public log_flag {
  public:
    explicit hour12_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(hour12_flag)

    ~hour12_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<hour12_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(2, pad_, dest);
        detail::pad2(detail::to_12_hour(tm_time), dest);
    }
};

// 分钟 flag "%M"
template <scoped_padder_type ScopedPadder>
class min_flag final : public log_flag {
  public:
    explicit min_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(min_flag)

    ~min_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<min_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(2, pad_, dest);
        detail::pad2(tm_time.tm_min, dest);
    }
};

// 秒 flag "%S"
template <scoped_padder_type ScopedPadder>
class second_flag final : public log_flag {
  public:
    explicit second_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(second_flag)

    ~second_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<second_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(2, pad_, dest);
        detail::pad2(tm_time.tm_sec, dest);
    }
};

// 毫秒 flag "%e"
template <scoped_padder_type ScopedPadder>
class millis_flag final : public log_flag {
  public:
    explicit millis_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(millis_flag)

    ~millis_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<millis_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(3, pad_, dest);
        const auto millis = time_fraction<std::chrono::milliseconds>(msg.point);
        detail::pad3(static_cast<int32_t>(millis.count()), dest);
    }
};

// 微秒 flag "%f"
template <scoped_padder_type ScopedPadder>
class micros_flag final : public log_flag {
  public:
    explicit micros_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(micros_flag)

    ~micros_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<micros_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(6, pad_, dest);
        const auto micros = time_fraction<std::chrono::microseconds>(msg.point);
        detail::pad_int(static_cast<size_t>(micros.count()), 6, dest);
    }
};

// 纳秒 flag "%F"
template <scoped_padder_type ScopedPadder>
class nanosecond_flag final : public log_flag {
  public:
    explicit nanosecond_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(nanosecond_flag)

    ~nanosecond_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<nanosecond_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(9, pad_, dest);
        const auto ns = time_fraction<std::chrono::nanoseconds>(msg.point);
        detail::pad_int(static_cast<size_t>(ns.count()), 9, dest);
    }
};

// 秒时间戳 flag "%E"
template <scoped_padder_type ScopedPadder>
class timestamp_flag final : public log_flag {
  public:
    explicit timestamp_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(timestamp_flag)

    ~timestamp_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<timestamp_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(10, pad_, dest);
        using namespace std::chrono;
        const auto count = duration_cast<seconds>(msg.point.time_since_epoch()).count();
        dest.append(fmt::format_int(count));
    }
};

// 上午下午 flag "%p"
template <scoped_padder_type ScopedPadder>
class noon_flag final : public log_flag {
  public:
    explicit noon_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(noon_flag)

    ~noon_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<noon_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto strv = detail::am_or_pm(tm_time);
        ScopedPadder p(strv.size(), pad_, dest);
        dest.append(strv);
    }
};

// 12小时制时间(02:55:02 pm) flag "%r"
template <scoped_padder_type ScopedPadder>
class clock_hour12_flag final : public log_flag {
  public:
    explicit clock_hour12_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(clock_hour12_flag)

    ~clock_hour12_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<clock_hour12_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(11, pad_, dest);
        detail::pad2(detail::to_12_hour(tm_time), dest);
        dest.push_back(':');
        detail::pad2(tm_time.tm_min, dest);
        dest.push_back(':');
        detail::pad2(tm_time.tm_sec, dest);
        dest.push_back(' ');
        dest.append(detail::am_or_pm(tm_time));
    }
};

// 24小时制时间到分为止(12:55) flag "%R"
template <scoped_padder_type ScopedPadder>
class short_clock_hour24_flag final : public log_flag {
  public:
    explicit short_clock_hour24_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(short_clock_hour24_flag)

    ~short_clock_hour24_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<short_clock_hour24_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(5, pad_, dest);
        detail::pad2(tm_time.tm_hour, dest);
        dest.push_back(':');
        detail::pad2(tm_time.tm_min, dest);
    }
};

// 24小时制时间(12:55:33) flag "%T" or "%X"
template <scoped_padder_type ScopedPadder>
class clock_hour24_flag final : public log_flag {
  public:
    explicit clock_hour24_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(clock_hour24_flag)

    ~clock_hour24_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<clock_hour24_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(8, pad_, dest);
        detail::pad2(tm_time.tm_hour, dest);
        dest.push_back(':');
        detail::pad2(tm_time.tm_min, dest);
        dest.push_back(':');
        detail::pad2(tm_time.tm_sec, dest);
    }
};

// 时区(+02:00) flag "%z"
template <scoped_padder_type ScopedPadder>
class timezone_flag final : public log_flag {
  public:
    explicit timezone_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(timezone_flag)

    ~timezone_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override {
        auto ptr = std::make_unique<timezone_flag>(pad_);
        ptr->last_update_ = last_update_;
        ptr->offset_minutes_ = offset_minutes_;
        return ptr;
    }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(6, pad_, dest);
        auto total_minutes = get_cached_offset(msg, tm_time);
        if (total_minutes < 0) {
            total_minutes = -total_minutes;
            dest.push_back('-');
        } else {
            dest.push_back('+');
        }

        detail::pad2(total_minutes / 60, dest);
        dest.push_back(':');
        detail::pad2(total_minutes % 60, dest);
    }

  private:
    int get_cached_offset(const log_message &msg, const std::tm &tm_time) {
        // refresh every 10 seconds
        if (msg.point - last_update_ >= std::chrono::seconds(10)) {
            offset_minutes_ = utc_minutes_offset(msg.point, tm_time);
            last_update_ = msg.point;
        }
        return offset_minutes_;
    }

    log_clock_point last_update_{std::chrono::seconds(0)};
    int32_t offset_minutes_{0};
};

// 线程id flag "%t"
template <scoped_padder_type ScopedPadder>
class tid_flag final : public log_flag {
  public:
    explicit tid_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(tid_flag)

    ~tid_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<tid_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto field_size = ScopedPadder::count_digits(msg.tid);
        ScopedPadder p(field_size, pad_, dest);
        dest.append(fmt::format_int(msg.tid));
    }
};

// 进程id flag "%P"
template <scoped_padder_type ScopedPadder>
class pid_flag final : public log_flag {
  public:
    explicit pid_flag(const log_padding &pad) : log_flag(pad) { pid_ = os::pid(); }

    SIMPLE_NON_COPYABLE(pid_flag)

    ~pid_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<pid_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto field_size = ScopedPadder::count_digits(pid_);
        ScopedPadder p(field_size, pad_, dest);
        dest.append(fmt::format_int(pid_));
    }

  private:
    int32_t pid_;
};

// log内容 flag "%v"
template <scoped_padder_type ScopedPadder>
class log_value_flag final : public log_flag {
  public:
    explicit log_value_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(log_value_flag)

    ~log_value_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<log_value_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        ScopedPadder p(msg.buf.size(), pad_, dest);
        dest.append(msg.buf);
    }
};

// 转义符号 %  flag "%%"
class ch_flag final : public log_flag {
  public:
    explicit ch_flag(const char ch) : log_flag(), ch_(ch) {}

    SIMPLE_NON_COPYABLE(ch_flag)

    ~ch_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<ch_flag>(ch_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override { dest.push_back(ch_); }

  private:
    char ch_;
};

// 自定义的字符串，非格式化标记
class aggregate_flag final : public log_flag {
  public:
    explicit aggregate_flag() : log_flag() {}

    SIMPLE_NON_COPYABLE(aggregate_flag)

    ~aggregate_flag() noexcept override = default;

    void add_ch(char ch) { str_ += ch; }

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override {
        auto ptr = std::make_unique<aggregate_flag>();
        ptr->str_ = str_;
        return ptr;
    }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override { dest.append(str_); }

    void clear() noexcept { return str_.clear(); }

    [[nodiscard]] bool empty() const noexcept { return str_.empty(); }

  private:
    std::string str_;
};

// 颜色输出起始点 flag "%^"
class color_start_flag final : public log_flag {
  public:
    explicit color_start_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(color_start_flag)

    ~color_start_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<color_start_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override { msg.color_start = dest.size(); }
};

// 颜色输出终点 flag "%$"
class color_stop_flag final : public log_flag {
  public:
    explicit color_stop_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(color_stop_flag)

    ~color_stop_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<color_stop_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override { msg.color_stop = dest.size(); }
};

// 代码位置(/dir/my_file.cpp:123) flag "%@"
template <scoped_padder_type ScopedPadder>
class source_location_flag final : public log_flag {
  public:
    explicit source_location_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(source_location_flag)

    ~source_location_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<source_location_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const char *file_name = msg.source.file_name();
        if (file_name[0] == 0) {
            ScopedPadder p(0, pad_, dest);
            return;
        }

        const auto file_name_size = std::char_traits<char>::length(file_name);
        const auto line = msg.source.line();
        size_t text_size;
        if (pad_.enabled) {
            text_size = file_name_size + ScopedPadder::count_digits(line) + 1;
        } else {
            text_size = 0;
        }
        ScopedPadder p(text_size, pad_, dest);
        dest.append(file_name, file_name + file_name_size);
        dest.push_back(':');
        dest.append(fmt::format_int(line));
    }
};

// 代码文件 flag "%g"
template <scoped_padder_type ScopedPadder>
class filename_flag final : public log_flag {
  public:
    explicit filename_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(filename_flag)

    ~filename_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<filename_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const char *file_name = msg.source.file_name();
        if (file_name[0] == 0) {
            ScopedPadder p(0, pad_, dest);
            return;
        }

        const auto file_name_size = std::char_traits<char>::length(file_name);
        const auto text_size = pad_.enabled ? file_name_size : 0;
        ScopedPadder p(text_size, pad_, dest);
        dest.append(file_name, file_name + file_name_size);
    }
};

// 简短的代码文件 flag "%s"
template <scoped_padder_type ScopedPadder>
class short_filename_flag final : public log_flag {
  public:
    explicit short_filename_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(short_filename_flag)

    ~short_filename_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<short_filename_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        std::string_view file_name = msg.source.file_name();
        if (file_name.empty()) {
            ScopedPadder p(0, pad_, dest);
            return;
        }

        if (const auto pos = file_name.find_last_of("/\\"); pos != std::string_view::npos) {
            file_name = file_name.substr(pos + 1);
        }
        const auto text_size = pad_.enabled ? file_name.size() : 0;
        ScopedPadder p(text_size, pad_, dest);
        dest.append(file_name);
    }
};

// 代码行数 flag "%#"
template <scoped_padder_type ScopedPadder>
class line_flag final : public log_flag {
  public:
    explicit line_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(line_flag)

    ~line_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<line_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const auto line = msg.source.line();
        if (line == 0) {
            ScopedPadder p(0, pad_, dest);
            return;
        }
        auto field_size = ScopedPadder::count_digits(line);
        ScopedPadder p(field_size, pad_, dest);
        dest.append(fmt::format_int(line));
    }
};

// 函数 flag "%!"
template <scoped_padder_type ScopedPadder>
class func_flag final : public log_flag {
  public:
    explicit func_flag(const log_padding &pad) : log_flag(pad) {}

    SIMPLE_NON_COPYABLE(func_flag)

    ~func_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<func_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        const std::string_view func = msg.source.function_name();
        if (func.empty()) {
            ScopedPadder p(0, pad_, dest);
            return;
        }

        const auto text_size = pad_.enabled ? func.size() : 0;
        ScopedPadder p(text_size, pad_, dest);
        dest.append(func);
    }
};

// 与上条消息的间隔时间 flag "%o"单位毫秒，"%i"单位微秒，"%u"单位纳秒，"%O"单位秒
template <scoped_padder_type ScopedPadder, typename DurationUnits>
class elapsed_time_flag final : public log_flag {
  public:
    explicit elapsed_time_flag(const log_padding &pad) : log_flag(pad), last_message_time_(std::chrono::system_clock::now()) {}

    SIMPLE_NON_COPYABLE(elapsed_time_flag)

    ~elapsed_time_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<elapsed_time_flag>(pad_); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        using std::chrono::duration_cast;
        using std::chrono::system_clock;

        const auto delta = (std::max)(msg.point - last_message_time_, system_clock::duration::zero());
        last_message_time_ = msg.point;
        const auto delta_units = duration_cast<DurationUnits>(delta);
        const auto delta_count = static_cast<size_t>(delta_units.count());
        const auto field_size = ScopedPadder::count_digits(delta_count);
        ScopedPadder p(field_size, pad_, dest);
        dest.append(fmt::format_int(delta_count));
    }

  private:
    log_clock_point last_message_time_;
};

// 默认打印格式
class default_flag final : public log_flag {
  public:
    explicit default_flag() : log_flag() {}

    SIMPLE_NON_COPYABLE(default_flag)

    ~default_flag() noexcept override = default;

    [[nodiscard]] std::unique_ptr<log_flag> clone() const override { return std::make_unique<default_flag>(); }

    void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) override {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        using std::chrono::seconds;

        // 时间
        const auto duration = msg.point.time_since_epoch();
        const auto secs = duration_cast<seconds>(duration);
        cache_time(secs, tm_time);
        dest.append(cached_datetime_);
        const auto millis = time_fraction<milliseconds>(msg.point);
        detail::pad3(static_cast<int32_t>(millis.count()), dest);
        dest.push_back(']');
        dest.push_back(' ');

        // 如果定义了log名字，打印log名字
        if (msg.ptr && !msg.ptr->name().empty()) {
            dest.push_back('[');
            dest.append(msg.ptr->name());
            dest.push_back(']');
            dest.push_back(' ');
        }

        // 打印log等级
        dest.push_back('[');
        msg.color_start = dest.size();
        dest.append(to_string_view(msg.level));
        msg.color_stop = dest.size();
        dest.push_back(']');
        dest.push_back(' ');

        // 代码位置
        std::string_view file_name = msg.source.file_name();
        if (!file_name.empty()) {
            dest.push_back('[');
            if (const auto pos = file_name.find_last_of("/\\"); pos != std::string_view::npos) {
                file_name = file_name.substr(pos + 1);
            }
            dest.append(file_name);
            dest.push_back(':');
            dest.append(fmt::format_int(msg.source.line()));
            dest.push_back(']');
            dest.push_back(' ');
        }

        // log 内容
        dest.append(msg.buf);
    }

  private:
    void cache_time(const std::chrono::seconds &secs, const std::tm &tm_time) {
        if (cache_timestamp_ == secs && cached_datetime_.size() > 0) {
            return;
        }

        cached_datetime_.clear();
        cached_datetime_.push_back('[');
        cached_datetime_.append(fmt::format_int(tm_time.tm_year + 1900));
        cached_datetime_.push_back('-');

        detail::pad2(tm_time.tm_mon + 1, cached_datetime_);
        cached_datetime_.push_back('-');

        detail::pad2(tm_time.tm_mday, cached_datetime_);
        cached_datetime_.push_back(' ');

        detail::pad2(tm_time.tm_hour, cached_datetime_);
        cached_datetime_.push_back(':');

        detail::pad2(tm_time.tm_min, cached_datetime_);
        cached_datetime_.push_back(':');

        detail::pad2(tm_time.tm_sec, cached_datetime_);
        cached_datetime_.push_back('.');

        cache_timestamp_ = secs;
    }

    std::chrono::seconds cache_timestamp_{0};
    log_buf_t cached_datetime_;
};

}  // namespace simple
