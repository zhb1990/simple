#pragma once
#include <simple/log/flag.h>
#include <simple/log/types.h>

#include <functional>
#include <vector>

namespace simple {

class log_formatter {
  public:
    SIMPLE_API log_formatter();

    SIMPLE_API explicit log_formatter(log_time_type tp);

    SIMPLE_API explicit log_formatter(const std::string_view &pattern);

    SIMPLE_API log_formatter(const std::string_view &pattern, log_time_type tp);

    SIMPLE_NON_COPYABLE(log_formatter)

    ~log_formatter() noexcept = default;

    SIMPLE_API void set_pattern(const std::string_view &pattern);

    SIMPLE_API void format(const log_message &msg, log_buf_t &dest);

    [[nodiscard]] SIMPLE_API std::unique_ptr<log_formatter> clone() const;

    using create_flag_func_t = std::function<std::unique_ptr<log_flag>(const log_padding &)>;

    SIMPLE_API log_formatter &add_custom_flag(char ch, create_flag_func_t func);

  private:
    template <scoped_padder_type Padder>
    void handle_flag(char flag, log_padding padding);

    log_time_type time_type_;
    bool need_localtime_;
    std::tm cached_tm_{};
    std::chrono::seconds last_log_secs_{0};
    std::unordered_map<char, create_flag_func_t> custom_flags_;
    std::vector<std::unique_ptr<log_flag>> formatters_;
};

}  // namespace simple
