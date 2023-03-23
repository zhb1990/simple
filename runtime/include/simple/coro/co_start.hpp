#pragma once

#include <simple/coro/scheduler.h>

#include <simple/coro/detached_task.hpp>
#include <simple/utils/coro_traits.hpp>
#include <simple/utils/type_traits.hpp>

namespace simple {

template <bool Delay = false, is_continuation_func F>
void co_start(F&& callback, cancellation_token token = {}) {
    using func_t = std::remove_cvref_t<F>;
    func_t temp = std::forward<F>(callback);
    if constexpr (Delay) {
        scheduler::instance().post([func_temp = std::move(temp), token_temp = std::move(token)]() mutable {
            if (token_temp.is_cancellation_requested()) {
                return;
            }

            auto launch = [](func_t func, cancellation_token token) -> detached_task {
                co_await set_cancellation_token_awaiter{std::move(token)};
                co_await func();
            };

            [[maybe_unused]] auto detached = launch(std::move(func_temp), std::move(token_temp));
        });
    } else {
        scheduler::instance().post_immediate([func_temp = std::move(temp), token_temp = std::move(token)]() mutable {
            if (token_temp.is_cancellation_requested()) {
                return;
            }

            auto launch = [](func_t func, cancellation_token token) -> detached_task {
                co_await set_cancellation_token_awaiter{std::move(token)};
                co_await func();
            };

            [[maybe_unused]] auto detached = launch(std::move(func_temp), std::move(token_temp));
        });
    }
}

template <bool Delay = false, is_continuation_func F>
requires(is_normal_func<F> && std::is_pointer_v<F>)
void co_start(F& callback, cancellation_token token = {}) {
    if constexpr (Delay) {
        scheduler::instance().post([callback, token_temp = std::move(token)]() mutable {
            if (token_temp.is_cancellation_requested()) {
                return;
            }

            auto launch = [](F& func, cancellation_token token) -> detached_task {
                co_await set_cancellation_token_awaiter{std::move(token)};
                co_await func();
            };

            [[maybe_unused]] auto detached = launch(callback, std::move(token_temp));
        });
    } else {
        scheduler::instance().post_immediate([callback, token_temp = std::move(token)]() mutable {
            if (token_temp.is_cancellation_requested()) {
                return;
            }

            auto launch = [](F& func, cancellation_token token) -> detached_task {
                co_await set_cancellation_token_awaiter{std::move(token)};
                co_await func();
            };

            [[maybe_unused]] auto detached = launch(callback, std::move(token_temp));
        });
    }
}

template <bool Delay = false, is_continuation_func F>
requires(is_normal_func<F> && !std::is_pointer_v<F>)
void co_start(F& callback, cancellation_token token = {}) {
    if constexpr (Delay) {
        scheduler::instance().post([&callback, token_temp = std::move(token)]() mutable {
            if (token_temp.is_cancellation_requested()) {
                return;
            }

            auto launch = [](F& func, cancellation_token token) -> detached_task {
                co_await set_cancellation_token_awaiter{std::move(token)};
                co_await func();
            };

            [[maybe_unused]] auto detached = launch(callback, std::move(token_temp));
        });
    } else {
        scheduler::instance().post_immediate([&callback, token_temp = std::move(token)]() mutable {
            if (token_temp.is_cancellation_requested()) {
                return;
            }

            auto launch = [](F& func, cancellation_token token) -> detached_task {
                co_await set_cancellation_token_awaiter{std::move(token)};
                co_await func();
            };

            [[maybe_unused]] auto detached = launch(callback, std::move(token_temp));
        });
    }
}

}  // namespace simple
