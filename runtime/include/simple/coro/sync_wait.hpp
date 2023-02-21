#pragma once
#include <simple/coro/scheduler.h>

#include <future>
#include <simple/coro/detached_task.hpp>
#include <simple/utils/coro_traits.hpp>
#include <simple/utils/type_traits.hpp>

namespace simple {

// 只能在调度线程外使用
template <is_awaitable Awaitable>
auto sync_wait(Awaitable&& awaitable, cancellation_token token = {}) {
    using result_t = remove_rvalue_reference_t<awaitable_result_t<Awaitable&&>>;
    std::promise<result_t> promise;
    auto future = promise.get_future();

    scheduler::instance().post([&] {
        auto launch = [](Awaitable&& awaitable, std::promise<result_t>& promise, cancellation_token token) -> detached_task {
            co_await set_cancellation_token_awaiter{std::move(token)};
            try {
                if constexpr (std::same_as<result_t, void>) {
                    co_await std::forward<Awaitable>(awaitable);
                    promise.set_value();
                } else {
                    promise.set_value(co_await std::forward<Awaitable>(awaitable));
                }
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        };

        [[maybe_unused]] auto detached = launch(std::forward<Awaitable>(awaitable), promise, std::move(token));
    });

    return future.get();
}

}  // namespace simple
