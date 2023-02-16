#pragma once

#include <concepts>
#include <coroutine>
#include <type_traits>
#include <utility>

namespace simple {

template <typename Awaitable>
concept global_co_await_awaitable = requires(Awaitable&& awaitable) { operator co_await(static_cast<Awaitable&&>(awaitable)); };

template <typename Awaitable>
concept member_co_await_awaitable =
    requires(Awaitable&& awaitable) { static_cast<Awaitable&&>(awaitable).operator co_await(); };

template <typename Awaitable>
concept is_awaiter = requires(Awaitable&& awaitable) {
                         static_cast<Awaitable&&>(awaitable).await_ready();
                         static_cast<Awaitable&&>(awaitable).await_suspend(std::declval<std::coroutine_handle<>>());
                         static_cast<Awaitable&&>(awaitable).await_resume();
                     };

template <typename Awaitable>
concept is_awaitable = global_co_await_awaitable<Awaitable> || member_co_await_awaitable<Awaitable> || is_awaiter<Awaitable>;

template <typename Awaitable>
concept is_awaitable_unwrap = is_awaitable<std::unwrap_reference_t<std::remove_reference_t<Awaitable>>>;

template <global_co_await_awaitable Awaitable>
auto get_awaiter(Awaitable&& awaitable) noexcept(noexcept(operator co_await(static_cast<Awaitable&&>(awaitable)))) {
    return operator co_await(static_cast<Awaitable&&>(awaitable));
}

template <member_co_await_awaitable Awaitable>
auto get_awaiter(Awaitable&& awaitable) noexcept(noexcept(static_cast<Awaitable&&>(awaitable).operator co_await())) {
    return static_cast<Awaitable&&>(awaitable).operator co_await();
}

template <typename Awaitable>
requires(!global_co_await_awaitable<Awaitable> && !member_co_await_awaitable<Awaitable> && is_awaiter<Awaitable>)
auto get_awaiter(Awaitable&& awaitable) noexcept {
    return static_cast<Awaitable&&>(awaitable);
}

template <is_awaitable Awaitable>
struct awaitable_traits {
    using awaiter_t = decltype(get_awaiter(std::declval<Awaitable>()));
    using result_t = decltype(std::declval<awaiter_t>().await_resume());
};

template <is_awaitable Awaitable>
using awaitable_awaiter_t = typename awaitable_traits<Awaitable>::awaiter_t;

template <is_awaitable Awaitable>
using awaitable_result_t = typename awaitable_traits<Awaitable>::result_t;

template <typename Func, typename... Args>
concept is_continuation_func = requires(Func&& func, Args&&... args) {
                                   { std::forward<Func>(func)(std::forward<Args>(args)...) } -> is_awaitable;
                               };

}  // namespace simple
