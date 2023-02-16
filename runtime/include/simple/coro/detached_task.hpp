#pragma once
#include <simple/coro/cancellation_token.h>

namespace simple {

struct detached_task {
    struct promise_type : promise_cancellation {
        // ReSharper disable once CppMemberFunctionMayBeStatic
        std::suspend_never initial_suspend() noexcept { return {}; }

        // ReSharper disable once CppMemberFunctionMayBeStatic
        std::suspend_never final_suspend() noexcept { return {}; }

        // ReSharper disable once CppMemberFunctionMayBeStatic
        void return_void() noexcept {}

        // ReSharper disable once CppMemberFunctionMayBeStatic
        void unhandled_exception() {}

        // ReSharper disable once CppMemberFunctionMayBeStatic
        detached_task get_return_object() noexcept { return detached_task{}; }
    };
};

}  // namespace simple
