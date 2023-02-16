#pragma once

#include <simple/application/application.h>
#include <simple/config.h>
#include <simple/coro/cancellation_registration.h>
#include <simple/coro/cancellation_token.h>
#include <simple/coro/scheduler.h>

#include <coroutine>
#include <optional>

namespace simple {

class frame_awaiter {
  public:
    DS_API explicit frame_awaiter(uint64_t frame);

    frame_awaiter(const frame_awaiter&) = delete;

    DS_API frame_awaiter(frame_awaiter&& other) noexcept;

    ~frame_awaiter() noexcept = default;

    frame_awaiter& operator=(frame_awaiter&) = delete;

    frame_awaiter& operator=(frame_awaiter&&) noexcept = delete;

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        handle_ = handle;
        token_ = handle.promise().get_cancellation_token();
        auto& app = application::instance();
        if (token_.is_cancellation_requested() || app.frame() == frame_) {
            return false;
        }

        if (token_.can_be_cancelled()) {
            registration_.emplace(token_, [this, &app]() {
                if (app.remove_frame_coroutine(frame_, handle_)) {
                    scheduler::instance().wake_up_coroutine(handle_);
                }
            });
        }
        app.wait_frame(frame_, handle_);
        return true;
    }

    DS_API void await_resume();

  private:
    uint64_t frame_{0};
    cancellation_token token_;
    std::optional<cancellation_registration> registration_;
    std::coroutine_handle<> handle_;
};

DS_API frame_awaiter skip_frame(uint64_t skip);

}  // namespace simple
