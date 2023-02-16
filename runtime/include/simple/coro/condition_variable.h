#pragma once
#include <simple/config.h>
#include <simple/coro/cancellation_registration.h>
#include <simple/coro/cancellation_token.h>
#include <simple/coro/scheduler.h>

#include <coroutine>
#include <memory>
#include <optional>
#include <simple/coro/task.hpp>

namespace simple {

class condition_variable_awaiter;
class condition_variable;

class condition_variable_state {
  public:
    condition_variable_state() = default;

    DS_NON_COPYABLE(condition_variable_state)

    ~condition_variable_state() noexcept = default;

    DS_API void add_awaiter(condition_variable_awaiter* awaiter) noexcept;

    DS_API void remove_awaiter(condition_variable_awaiter* awaiter) noexcept;

    [[nodiscard]] DS_API bool has_awaiter(condition_variable_awaiter* awaiter) const noexcept;

    void notify_one() noexcept;

    void notify_all() noexcept;

  private:
    condition_variable_awaiter* header_{nullptr};
    condition_variable_awaiter* tail_{nullptr};
};

class condition_variable_awaiter {
    condition_variable_awaiter() = default;

  public:
    DS_API condition_variable_awaiter(condition_variable_awaiter&& other) noexcept;

    condition_variable_awaiter(const condition_variable_awaiter&) = delete;

    ~condition_variable_awaiter() noexcept = default;

    condition_variable_awaiter& operator=(condition_variable_awaiter&) = delete;

    condition_variable_awaiter& operator=(condition_variable_awaiter&&) noexcept = delete;

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        handle_ = handle;
        token_ = handle.promise().get_cancellation_token();

        if (token_.is_cancellation_requested() || (!state_)) {
            return false;
        }

        state_->add_awaiter(this);

        if (token_.can_be_cancelled()) {
            auto& scheduler = scheduler::instance();
            registration_.emplace(token_, [this, &scheduler]() {
                if (state_->has_awaiter(this)) {
                    state_->remove_awaiter(this);
                    scheduler.wake_up_coroutine(handle_);
                }
            });
        }

        return true;
    }

    DS_API void await_resume();

  protected:
    friend class condition_variable_state;
    friend class condition_variable;

    std::shared_ptr<condition_variable_state> state_;
    cancellation_token token_;
    std::optional<cancellation_registration> registration_;
    std::coroutine_handle<> handle_;

    condition_variable_awaiter* next_{nullptr};
    condition_variable_awaiter* prev_{nullptr};
};

class condition_variable {
  public:
    DS_API condition_variable();

    DS_COPYABLE_DEFAULT(condition_variable)

    ~condition_variable() noexcept = default;

    [[nodiscard]] DS_API condition_variable_awaiter wait() const noexcept;

    DS_API void notify_one() const noexcept;

    DS_API void notify_all() const noexcept;

  private:
    std::shared_ptr<condition_variable_state> state_;
};

}  // namespace simple
