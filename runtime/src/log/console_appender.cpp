#include <simple/log/console_appender.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <array>

namespace simple {

static std::mutex console_write_mutex;

constexpr std::string_view console_color_reset = "\033[m";
constexpr std::string_view console_color_white = "\033[37m";
constexpr std::string_view console_color_green = "\033[32m";
constexpr std::string_view console_color_cyan = "\033[36m";
constexpr std::string_view console_color_yellow_bold = "\033[33m\033[1m";
constexpr std::string_view console_color_red_bold = "\033[31m\033[1m";
constexpr std::string_view console_color_bold_on_red = "\033[1m\033[41m";

constexpr std::array log_level_to_color{console_color_reset,       console_color_bold_on_red, console_color_red_bold,
                                        console_color_yellow_bold, console_color_green,       console_color_cyan,
                                        console_color_white};

console_appender::console_appender(FILE* target_file) : file_(target_file) {
    enable_color_ = os::is_color_terminal() && os::in_terminal(file_);
}

void console_appender::write(log_level level, const log_clock_point&, const log_buf_t& buf, size_t color_start,
                             size_t color_stop) {
    std::lock_guard lock(console_write_mutex);
    if (enable_color_ && color_stop > color_start) {
        // before color range
        write_range(buf, 0, color_start);
        // in color range
        write_strv(log_level_to_color[static_cast<size_t>(level)]);
        write_range(buf, color_start, color_stop);
        write_strv(console_color_reset);
        // after color range
        write_range(buf, color_stop, buf.size());
    } else {
        write_range(buf, 0, buf.size());
    }
    fflush(file_);  // NOLINT(cert-err33-c)
}

void console_appender::flush_unlock() {
    fflush(file_);  // NOLINT(cert-err33-c)
}

void write_console(const std::string_view& strv, FILE* file) {
    std::lock_guard lock(console_write_mutex);
    fwrite(strv.data(), sizeof(char), strv.size(), file);  // NOLINT(cert-err33-c)
}

// ReSharper disable once CppMemberFunctionMayBeConst
void console_appender::write_range(const log_buf_t& formatted, size_t start, size_t end) {
    fwrite(formatted.data() + start, sizeof(char), end - start, file_);  // NOLINT(cert-err33-c)
}

// ReSharper disable once CppMemberFunctionMayBeConst
void console_appender::write_strv(const std::string_view& strv) {
    fwrite(strv.data(), sizeof(char), strv.size(), file_);  // NOLINT(cert-err33-c)
}

stdout_appender::stdout_appender() : console_appender(stdout) {}

stderr_appender::stderr_appender() : console_appender(stderr) {}

}  // namespace simple
