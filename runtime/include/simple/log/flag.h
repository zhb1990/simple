#pragma once
#include <simple/log/types.h>

namespace simple {

struct log_padding {
    enum class pad_side { left, right, center };
    size_t width{0};
    pad_side side{pad_side::left};
    bool truncate{false};
    bool enabled{false};
};

template <typename T>
concept scoped_padder_type = requires {
                                 requires std::constructible_from<T, size_t, const log_padding &, log_buf_t &>;
                                 { T::count_digits(size_t{}) } -> std::same_as<int>;
                             };

class log_flag {
  public:
    log_flag() = default;

    explicit log_flag(const log_padding &pad) : pad_(pad) {}

    DS_NON_COPYABLE(log_flag)

    virtual ~log_flag() noexcept = default;

    [[nodiscard]] virtual std::unique_ptr<log_flag> clone() const = 0;

    virtual void format(const log_message &msg, const std::tm &tm_time, log_buf_t &dest) = 0;

  protected:
    log_padding pad_;
};

}  // namespace simple
