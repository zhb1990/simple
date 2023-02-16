#pragma once
#include <coroutine>

namespace simple {

struct current_coroutine {
    std::coroutine_handle<void> current;

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    bool await_suspend(std::coroutine_handle<void> handle) noexcept {
        current = handle;
        return false;
    }

    [[nodiscard]] std::coroutine_handle<void> await_resume() const noexcept { return current; }
};

}  // namespace simple
