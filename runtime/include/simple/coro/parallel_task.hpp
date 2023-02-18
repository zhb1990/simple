#pragma once
#include <simple/coro/cancellation_registration.h>
#include <simple/coro/cancellation_source.h>
#include <simple/coro/cancellation_token.h>
#include <simple/error.h>

#include <coroutine>
#include <exception>
#include <limits>
#include <optional>
#include <simple/utils/coro_traits.hpp>
#include <simple/utils/type_traits.hpp>
#include <tuple>
#include <utility>

namespace simple {

template <typename T>
class parallel_task;

template <typename Tasks>
class parallel_task_ready_awaitable;

enum class wait_type { wait_all, wait_one, wait_one_fail, wait_one_succ };

class parallel_counter {
  public:
    parallel_counter(size_t count, wait_type tp) noexcept : count_(count + 1), type_(tp) {}

    [[nodiscard]] bool is_ready() const noexcept { return static_cast<bool>(handle_); }

    bool try_await(std::coroutine_handle<> handle) noexcept {
        handle_ = handle;
        return count_-- > 1;
    }

    void notify(bool has_exception) noexcept {
        if (type_ == wait_type::wait_one) {
            if (!source_.is_cancellation_requested()) {
                source_.request_cancellation();
            }
        } else if (type_ == wait_type::wait_one_fail) {
            if (has_exception && !source_.is_cancellation_requested()) {
                source_.request_cancellation();
            }
        } else if (type_ == wait_type::wait_one_succ) {
            if (!has_exception && !source_.is_cancellation_requested()) {
                source_.request_cancellation();
            }
        }

        if (count_-- == 1) {
            handle_.resume();
        }
    }

    [[nodiscard]] auto get_token() const noexcept { return source_.token(); }

    [[nodiscard]] auto get_type() const noexcept { return type_; }

    void request_cancellation() const { source_.request_cancellation(); }

    size_t increment_index() noexcept { return index_++; }

  protected:
    size_t count_;
    wait_type type_;
    cancellation_source source_;
    std::coroutine_handle<> handle_;
    size_t index_{0};
};

template <typename T>
class parallel_task_promise : public promise_cancellation {
  public:
    using coroutine_handle_t = std::coroutine_handle<parallel_task_promise>;

    parallel_task_promise() noexcept = default;

    auto get_return_object() noexcept { return coroutine_handle_t::from_promise(*this); }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }

    [[nodiscard]] auto final_suspend() const noexcept {
        class completion_notifier {
          public:
            // ReSharper disable once CppMemberFunctionMayBeStatic
            [[nodiscard]] bool await_ready() const noexcept { return false; }

            // ReSharper disable once CppMemberFunctionMayBeStatic
            void await_suspend(coroutine_handle_t coro) const noexcept {
                auto& promise = coro.promise();
                promise.index_ = promise.counter_->increment_index();
                promise.counter_->notify(static_cast<bool>(promise.get_exception()));
            }

            // ReSharper disable once CppMemberFunctionMayBeStatic
            void await_resume() const noexcept {}
        };

        return completion_notifier{};
    }

    void unhandled_exception() noexcept { exception_ = std::current_exception(); }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    void return_void() noexcept {}

    auto yield_value(T&& result) noexcept {
        result_ = std::addressof(result);
        return final_suspend();
    }

    void start(parallel_counter& counter) noexcept {
        counter_ = &counter;
        token = counter.get_token();
        coroutine_handle_t::from_promise(*this).resume();
    }

    T& result() & {
        rethrow_if_exception();
        return *result_;
    }

    T&& result() && {
        rethrow_if_exception();
        return std::forward<T>(*result_);
    }

    [[nodiscard]] std::exception_ptr get_exception() const noexcept { return exception_; }

    [[nodiscard]] size_t get_index() const noexcept { return index_; }

  private:
    void rethrow_if_exception() const {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    parallel_counter* counter_{nullptr};
    std::exception_ptr exception_;
    std::add_pointer_t<T> result_{nullptr};
    size_t index_{std::numeric_limits<size_t>::max()};
};

template <>
class parallel_task_promise<void> : public promise_cancellation {
  public:
    using coroutine_handle_t = std::coroutine_handle<parallel_task_promise>;

    parallel_task_promise() noexcept = default;

    auto get_return_object() noexcept { return coroutine_handle_t::from_promise(*this); }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] std::suspend_always initial_suspend() noexcept { return {}; }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] auto final_suspend() const noexcept {
        class completion_notifier {
          public:
            // ReSharper disable once CppMemberFunctionMayBeStatic
            [[nodiscard]] bool await_ready() const noexcept { return false; }

            // ReSharper disable once CppMemberFunctionMayBeStatic
            void await_suspend(coroutine_handle_t coro) const noexcept {
                auto& promise = coro.promise();
                promise.index_ = promise.counter_->increment_index();
                promise.counter_->notify(static_cast<bool>(promise.get_exception()));
            }

            // ReSharper disable once CppMemberFunctionMayBeStatic
            void await_resume() const noexcept {}
        };

        return completion_notifier{};
    }

    void unhandled_exception() noexcept { exception_ = std::current_exception(); }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    void return_void() noexcept {}

    void start(parallel_counter& counter) noexcept {
        counter_ = &counter;
        token = counter.get_token();
        coroutine_handle_t::from_promise(*this).resume();
    }

    void result() const {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    [[nodiscard]] std::exception_ptr get_exception() const noexcept { return exception_; }

    [[nodiscard]] size_t get_index() const noexcept { return index_; }

  private:
    parallel_counter* counter_{nullptr};
    std::exception_ptr exception_;
    size_t index_{std::numeric_limits<size_t>::max()};
};

template <typename T>
class parallel_task {
  public:
    using value_type = T;
    using promise_type = parallel_task_promise<T>;

    using coroutine_handle_t = typename promise_type::coroutine_handle_t;

    parallel_task(coroutine_handle_t coroutine) noexcept : coroutine_(coroutine) {}

    parallel_task(parallel_task&& other) noexcept : coroutine_(std::exchange(other.coroutine_, coroutine_handle_t{})) {}

    ~parallel_task() {
        if (coroutine_) {
            coroutine_.destroy();
        }
    }

    parallel_task& operator=(parallel_task&& other) noexcept {
        if (std::addressof(other) != this) {
            if (coroutine_) {
                coroutine_.destroy();
            }

            coroutine_ = other.coroutine_;
            other.coroutine_ = nullptr;
        }

        return *this;
    }

    parallel_task(const parallel_task&) = delete;
    parallel_task& operator=(const parallel_task&) = delete;

    decltype(auto) result() & { return coroutine_.promise().result(); }

    decltype(auto) result() && { return std::move(coroutine_.promise()).result(); }

    [[nodiscard]] std::exception_ptr get_exception() const noexcept { return coroutine_.promise().get_exception(); }

    [[nodiscard]] size_t get_index() const noexcept { return coroutine_.promise().get_index(); }

  private:
    template <typename Tasks>
    friend class parallel_task_ready_awaitable;

    void start(parallel_counter& counter) noexcept { coroutine_.promise().start(counter); }

    coroutine_handle_t coroutine_;
};

template <is_awaitable Awaitable>
requires(!std::same_as<awaitable_result_t<Awaitable &&>, void>)
auto make_parallel_task(Awaitable awaitable) -> parallel_task<awaitable_result_t<Awaitable&&>> {
    if (auto token = co_await get_cancellation_token_awaiter{}; token.is_cancellation_requested()) [[unlikely]] {
        throw std::system_error(coro_errors::canceled);
    }

    co_yield co_await std::forward<Awaitable>(awaitable);
}

template <is_awaitable Awaitable>
requires(std::same_as<awaitable_result_t<Awaitable &&>, void>)
auto make_parallel_task(Awaitable awaitable) -> parallel_task<void> {
    if (auto token = co_await get_cancellation_token_awaiter{}; token.is_cancellation_requested()) [[unlikely]] {
        throw std::system_error(coro_errors::canceled);
    }

    co_await std::forward<Awaitable>(awaitable);
}

template <is_awaitable Awaitable>
requires(!std::same_as<awaitable_result_t<Awaitable&>, void>)
auto make_parallel_task(std::reference_wrapper<Awaitable> awaitable) -> parallel_task<awaitable_result_t<Awaitable&>> {
    if (auto token = co_await get_cancellation_token_awaiter{}; token.is_cancellation_requested()) [[unlikely]] {
        throw std::system_error(coro_errors::canceled);
    }

    co_yield co_await awaitable.get();
}

template <is_awaitable Awaitable>
requires(std::same_as<awaitable_result_t<Awaitable&>, void>)
auto make_parallel_task(std::reference_wrapper<Awaitable> awaitable) -> parallel_task<void> {
    if (auto token = co_await get_cancellation_token_awaiter{}; token.is_cancellation_requested()) [[unlikely]] {
        throw std::system_error(coro_errors::canceled);
    }

    co_await awaitable.get();
}

template <typename Tasks>
class parallel_task_ready_awaitable {
  public:
    explicit constexpr parallel_task_ready_awaitable(wait_type) noexcept {}

    // ReSharper disable thrice CppMemberFunctionMayBeStatic
    [[nodiscard]] constexpr bool await_ready() const noexcept { return true; }

    void await_suspend(std::coroutine_handle<>) noexcept {}

    void await_resume() const noexcept {}
};

template <>
class parallel_task_ready_awaitable<std::tuple<>> {
  public:
    constexpr parallel_task_ready_awaitable(wait_type, const std::tuple<>&) noexcept {}

    // ReSharper disable thrice CppMemberFunctionMayBeStatic
    [[nodiscard]] constexpr bool await_ready() const noexcept { return true; }

    void await_suspend(std::coroutine_handle<>) noexcept {}

    [[nodiscard]] std::tuple<> await_resume() const noexcept { return {}; }
};

template <typename T>
concept is_parallel_task = template_instant_of<parallel_task, T>;

template <is_parallel_task... Tasks>
class parallel_task_ready_awaitable<std::tuple<Tasks...>> {
  public:
    explicit parallel_task_ready_awaitable(wait_type tp, Tasks&&... tasks) noexcept(
        std::conjunction_v<std::is_nothrow_move_constructible<Tasks>...>)
        : counter_(sizeof...(Tasks), tp), tasks_(std::move(tasks)...) {}

    parallel_task_ready_awaitable(wait_type tp, std::tuple<Tasks...>&& tasks) noexcept(
        std::is_nothrow_move_constructible_v<std::tuple<Tasks...>>)
        : counter_(sizeof...(Tasks), tp), tasks_(std::move(tasks)) {}

    parallel_task_ready_awaitable(const parallel_task_ready_awaitable&) = delete;

    parallel_task_ready_awaitable(parallel_task_ready_awaitable&& other) noexcept
        : counter_(sizeof...(Tasks), other.counter_.get_type()), tasks_(std::move(other.tasks_)) {}

    ~parallel_task_ready_awaitable() noexcept = default;

    parallel_task_ready_awaitable& operator=(parallel_task_ready_awaitable&) = delete;

    parallel_task_ready_awaitable& operator=(parallel_task_ready_awaitable&&) noexcept = delete;

    struct awaiter_base {
        explicit awaiter_base(parallel_task_ready_awaitable* input) : awaitable(input) {}

        [[nodiscard]] bool await_ready() const noexcept { return awaitable->is_ready(); }

        template <typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
            token = handle.promise().get_cancellation_token();
            if (token.is_cancellation_requested()) {
                return false;
            }

            const auto ret = awaitable->try_await(handle);
            if (ret && token.can_be_cancelled()) {
                registration.emplace(token, [this] { awaitable->counter_.request_cancellation(); });
            }
            return ret;
        }

        parallel_task_ready_awaitable* awaitable{nullptr};
        cancellation_token token;
        std::optional<cancellation_registration> registration;
    };

    auto operator co_await() & noexcept {
        struct awaiter : awaiter_base {
            explicit awaiter(parallel_task_ready_awaitable* input) : awaiter_base(input) {}

            std::tuple<Tasks...>& await_resume() {
                awaiter_base::registration.reset();
                if (awaiter_base::token.is_cancellation_requested()) [[unlikely]] {
                    throw std::system_error(coro_errors::canceled);
                }

                return awaiter_base::awaitable->tasks_;
            }
        };

        return awaiter{this};
    }

    auto operator co_await() && noexcept {
        struct awaiter : awaiter_base {
            explicit awaiter(parallel_task_ready_awaitable* input) : awaiter_base(input) {}

            std::tuple<Tasks...>&& await_resume() {
                awaiter_base::registration.reset();
                if (awaiter_base::token.is_cancellation_requested()) [[unlikely]] {
                    throw std::system_error(coro_errors::canceled);
                }

                return std::move(awaiter_base::awaitable->tasks_);
            }
        };

        return awaiter{this};
    }

  private:
    [[nodiscard]] bool is_ready() const noexcept { return counter_.is_ready(); }

    bool try_await(std::coroutine_handle<> handle) noexcept {
        start_tasks(std::make_index_sequence<sizeof...(Tasks)>{});
        return counter_.try_await(handle);
    }

    template <size_t... Indices>
    void start_tasks(std::integer_sequence<size_t, Indices...>) noexcept {
        (std::get<Indices>(tasks_).start(counter_), ...);
    }

    parallel_counter counter_;
    std::tuple<Tasks...> tasks_;
};

template <is_container Tasks>
requires is_parallel_task<typename Tasks::value_type>
class parallel_task_ready_awaitable<Tasks> {
  public:
    explicit parallel_task_ready_awaitable(wait_type tp, Tasks&& tasks) noexcept
        : counter_(tasks.size(), tp), tasks_(std::move(tasks)) {}

    parallel_task_ready_awaitable(const parallel_task_ready_awaitable&) = delete;

    parallel_task_ready_awaitable(parallel_task_ready_awaitable&& other) noexcept
        : counter_(other.tasks_.size(), other.counter_.get_type()), tasks_(std::move(other.tasks_)) {}

    ~parallel_task_ready_awaitable() noexcept = default;

    parallel_task_ready_awaitable& operator=(parallel_task_ready_awaitable&) = delete;

    parallel_task_ready_awaitable& operator=(parallel_task_ready_awaitable&&) noexcept = delete;

    struct awaiter_base {
        explicit awaiter_base(parallel_task_ready_awaitable* input) : awaitable(input) {}

        [[nodiscard]] bool await_ready() const noexcept { return awaitable->is_ready(); }

        template <typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
            token = handle.promise().get_cancellation_token();
            if (token.is_cancellation_requested()) {
                return false;
            }

            const auto ret = awaitable->try_await(handle);
            if (ret && token.can_be_cancelled()) {
                registration.emplace(token, [this] { awaitable->counter_.request_cancellation(); });
            }
            return ret;
        }

        parallel_task_ready_awaitable* awaitable{nullptr};
        cancellation_token token;
        std::optional<cancellation_registration> registration;
    };

    auto operator co_await() & noexcept {
        struct awaiter : awaiter_base {
            explicit awaiter(parallel_task_ready_awaitable* input) : awaiter_base(input) {}

            Tasks& await_resume() {
                awaiter_base::registration.reset();
                if (awaiter_base::token.is_cancellation_requested()) [[unlikely]] {
                    throw std::system_error(coro_errors::canceled);
                }

                return awaiter_base::awaitable->tasks_;
            }
        };

        return awaiter{this};
    }

    auto operator co_await() && noexcept {
        struct awaiter : awaiter_base {
            explicit awaiter(parallel_task_ready_awaitable* input) : awaiter_base(input) {}

            Tasks&& await_resume() {
                awaiter_base::registration.reset();
                if (awaiter_base::token.is_cancellation_requested()) [[unlikely]] {
                    throw std::system_error(coro_errors::canceled);
                }

                return std::move(awaiter_base::awaitable->tasks_);
            }
        };

        return awaiter{this};
    }

  private:
    [[nodiscard]] bool is_ready() const noexcept { return counter_.is_ready(); }

    bool try_await(std::coroutine_handle<> handle) noexcept {
        for (auto&& task : tasks_) {
            task.start(counter_);
        }
        return counter_.try_await(handle);
    }

    parallel_counter counter_;
    Tasks tasks_;
};

template <is_awaitable_unwrap... Awaitables>
[[nodiscard]] auto when_ready(wait_type tp, Awaitables&&... awaitables) {
    return parallel_task_ready_awaitable<
        std::tuple<parallel_task<awaitable_result_t<std::unwrap_reference_t<std::remove_reference_t<Awaitables>>>>...>>(
        tp, std::make_tuple(make_parallel_task(std::forward<Awaitables>(awaitables))...));
}

template <typename Awaitables>
requires(is_awaitable<std::unwrap_reference_t<typename std::remove_cvref_t<Awaitables>::value_type>> &&
         is_container<std::remove_cvref_t<Awaitables>>)
[[nodiscard]] auto when_ready(wait_type tp, Awaitables&& awaitables) {
    using value_type = typename std::remove_cvref_t<Awaitables>::value_type;
    using result_t = awaitable_result_t<std::unwrap_reference_t<value_type>>;

    std::vector<parallel_task<result_t>> tasks;

    tasks.reserve(awaitables.size());
    for (auto& awaitable : awaitables) {
        tasks.emplace_back(make_parallel_task(std::move(awaitable)));
    }

    return parallel_task_ready_awaitable<std::vector<parallel_task<result_t>>>{tp, std::move(tasks)};
}

}  // namespace simple
