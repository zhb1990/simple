#pragma once
#include <simple/config.h>

#include <coroutine>
#include <memory>

namespace simple {

class cancellation_state;
class cancellation_source;
class cancellation_registration;

class cancellation_token {
  public:
    cancellation_token() = default;

    explicit cancellation_token(const cancellation_source& source);

    DS_COPYABLE_DEFAULT(cancellation_token)

    ~cancellation_token() noexcept = default;

    [[nodiscard]] DS_API bool can_be_cancelled() const noexcept;

    [[nodiscard]] DS_API bool is_cancellation_requested() const noexcept;

    explicit operator bool() const noexcept { return static_cast<bool>(state_); }

  private:
    friend class cancellation_registration;

    std::shared_ptr<cancellation_state> state_;
};

struct promise_cancellation {
    [[nodiscard]] cancellation_token get_cancellation_token() const noexcept { return token; }

    void set_cancellation_token(cancellation_token new_token) noexcept { token = std::move(new_token); }

    cancellation_token token;
};

struct set_cancellation_token_awaiter {
    cancellation_token token;

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    template <typename Promise>
    [[nodiscard]] bool await_suspend(std::coroutine_handle<Promise> handle) const noexcept {
        handle.promise().set_cancellation_token(token);
        return false;
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    void await_resume() noexcept {}
};

struct get_cancellation_token_awaiter {
    cancellation_token token;

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        token = handle.promise().get_cancellation_token();
        return false;
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    cancellation_token await_resume() noexcept { return std::move(token); }
};

}  // namespace simple
