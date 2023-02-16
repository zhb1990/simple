#pragma once

#include <simple/config.h>

namespace simple {

// 不保证线程安全，只能在scheduler所在的线程中使用。

class cancellation_registration;

class cancellation_state {
  public:
    cancellation_state();

    DS_NON_COPYABLE(cancellation_state)

    ~cancellation_state() noexcept = default;

    void register_callback(cancellation_registration* registration);

    void deregister_callback(cancellation_registration* registration) noexcept;

    [[nodiscard]] bool can_be_cancelled() const noexcept;

    [[nodiscard]] bool is_cancellation_requested() const noexcept;

    void request_cancellation();

  private:
    enum class state { none, cancelled };

    state state_;

    cancellation_registration* registrations_;
};

}  // namespace simple
