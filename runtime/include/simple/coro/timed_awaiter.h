#pragma once

#include <simple/config.h>
#include <simple/containers/time_queue.h>
#include <simple/coro/cancellation_registration.h>
#include <simple/coro/cancellation_token.h>
#include <simple/coro/scheduler.h>

#include <coroutine>
#include <optional>

namespace simple {

class timed_awaiter {
  public:
    using clock = timer_queue::clock;
    using time_point = timer_queue::time_point;
    using duration = timer_queue::duration;

    SIMPLE_API explicit timed_awaiter(time_point point);

    SIMPLE_API explicit timed_awaiter(duration dur);

    timed_awaiter(const timed_awaiter&) = delete;

    SIMPLE_API timed_awaiter(timed_awaiter&& other) noexcept;

    ~timed_awaiter() noexcept = default;

    timed_awaiter& operator=(timed_awaiter&) = delete;

    timed_awaiter& operator=(timed_awaiter&&) noexcept = delete;

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        handle_ = handle;
        token_ = handle.promise().get_cancellation_token();

        if (token_.is_cancellation_requested() || clock::now() >= node.point) {
            return false;
        }

        auto& scheduler = scheduler::instance();
        auto& queue = scheduler.get_timer_queue();
        if (token_.can_be_cancelled()) {
            registration_.emplace(token_, [this, &scheduler, &queue]() {
                if (queue.remove(&node)) {
                    scheduler.wake_up_coroutine(handle_);
                }
            });
        }

        queue.enqueue(&node);

        return true;
    }

    SIMPLE_API void await_resume();

    void wake_up() const;

    timer_queue::node node;

  private:
    cancellation_token token_;
    std::optional<cancellation_registration> registration_;
    std::coroutine_handle<> handle_;
};

inline timed_awaiter sleep_for(timer_queue::duration dur) { return timed_awaiter{dur}; }

inline timed_awaiter sleep_until(timer_queue::time_point point) { return timed_awaiter{point}; }

}  // namespace simple
