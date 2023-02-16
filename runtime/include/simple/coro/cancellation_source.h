#pragma once

#include <simple/config.h>
#include <simple/coro/cancellation_token.h>

#include <memory>

namespace simple {

class cancellation_state;

class cancellation_source {
  public:
    SIMPLE_API cancellation_source();

    SIMPLE_COPYABLE_DEFAULT(cancellation_source)

    ~cancellation_source() noexcept = default;

    [[nodiscard]] SIMPLE_API cancellation_token token() const noexcept;

    SIMPLE_API void request_cancellation() const;

    [[nodiscard]] SIMPLE_API bool can_be_cancelled() const noexcept;

    [[nodiscard]] SIMPLE_API bool is_cancellation_requested() const noexcept;

  private:
    friend class cancellation_token;

    std::shared_ptr<cancellation_state> state_;
};

}  // namespace simple
