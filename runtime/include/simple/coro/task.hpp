#pragma once

#include <simple/coro/cancellation_token.h>
#include <simple/error.h>

#include <cassert>
#include <concepts>
#include <coroutine>
#include <exception>
#include <variant>

namespace simple {

template <typename T = void>
class task;

class task_promise_base : public promise_cancellation {
  public:
    struct final_awaitable {
        // ReSharper disable twice CppMemberFunctionMayBeStatic
        [[nodiscard]] bool await_ready() const noexcept { return false; }

        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept {
            return h.promise().continuation_ ? h.promise().continuation_ : std::noop_coroutine();
        }

        void await_resume() noexcept {}
    };

    auto initial_suspend() noexcept { return std::suspend_always{}; }

    auto final_suspend() noexcept { return final_awaitable{}; }

    void set_continuation(std::coroutine_handle<> continuation) noexcept { continuation_ = continuation; }

  protected:
    std::coroutine_handle<> continuation_;
};

template <typename T>
class task_promise : public task_promise_base {
  public:
    task<T> get_return_object() noexcept;

    template <typename V>
    requires std::convertible_to<V&&, T>
    void return_value(V&& value) noexcept(std::is_nothrow_constructible_v<T, V&&>) {
        value_.template emplace<T>(std::forward<V>(value));
    }

    void unhandled_exception() noexcept { value_.template emplace<std::exception_ptr>(std::current_exception()); }

    T& result() & {
        if (std::holds_alternative<std::exception_ptr>(value_)) [[unlikely]] {
            std::rethrow_exception(std::get<std::exception_ptr>(value_));
        }

        assert(std::holds_alternative<T>(value_));
        return std::get<T>(value_);
    }

    T&& result() && {
        if (std::holds_alternative<std::exception_ptr>(value_)) [[unlikely]] {
            std::rethrow_exception(std::get<std::exception_ptr>(value_));
        }

        assert(std::holds_alternative<T>(value_));
        return std::move(std::get<T>(value_));
    }

  private:
    std::variant<std::monostate, T, std::exception_ptr> value_;
};

template <>
class task_promise<void> : public task_promise_base {
  public:
    task<void> get_return_object() noexcept;

    void return_void() noexcept {}

    void unhandled_exception() noexcept { exception_ = std::current_exception(); }

    void result() const {
        if (exception_ != nullptr) [[unlikely]] {
            std::rethrow_exception(exception_);
        }
    }

  private:
    std::exception_ptr exception_;
};

template <typename T>
class task_promise<T&> : public task_promise_base {
  public:
    task_promise() noexcept = default;

    task<T&> get_return_object() noexcept;

    void unhandled_exception() noexcept { value_.template emplace<std::exception_ptr>(std::current_exception()); }

    void return_value(T& value) noexcept { value_.template emplace<T*>(std::addressof(value)); }

    T& result() {
        if (std::holds_alternative<std::exception_ptr>(value_)) [[unlikely]] {
            std::rethrow_exception(std::get<std::exception_ptr>(value_));
        }

        assert(std::holds_alternative<T*>(value_));
        return *std::get<T*>(value_);
    }

  private:
    std::variant<std::monostate, T*, std::exception_ptr> value_;
};

template <typename T>
class task {
  public:
    using promise_type = task_promise<T>;
    using value_type = T;

    struct awaitable_base {
        explicit awaitable_base(std::coroutine_handle<promise_type> handle) noexcept : continuation(handle) {}

        [[nodiscard]] bool await_ready() const noexcept { return !continuation || continuation.done(); }

        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> handle) noexcept {
            auto& promise = continuation.promise();
            if (auto token = handle.promise().get_cancellation_token()) {
                if (token.is_cancellation_requested()) [[unlikely]] {
                    return handle;
                }

                promise.set_cancellation_token(token);
            }
            promise.set_continuation(handle);
            return continuation;
        }

        void check_token() const {
            if (auto token = continuation.promise().get_cancellation_token(); token.is_cancellation_requested()) [[unlikely]] {
                throw std::system_error(coro_errors::canceled);
            }
        }

        std::coroutine_handle<promise_type> continuation;
    };

    task() = default;

    explicit task(std::coroutine_handle<promise_type> continuation) : continuation_(continuation) {}

    task(task&& t) noexcept : continuation_(t.continuation_) { t.continuation_ = nullptr; }

    task(const task&) = delete;

    task& operator=(const task&) = delete;

    ~task() {
        if (continuation_) {
            continuation_.destroy();
        }
    }

    task& operator=(task&& other) noexcept {
        if (std::addressof(other) != this) {
            if (continuation_) {
                continuation_.destroy();
            }

            continuation_ = other.continuation_;
            other.continuation_ = nullptr;
        }

        return *this;
    }

    [[nodiscard]] bool is_ready() const noexcept { return !continuation_ || continuation_.done(); }

    auto operator co_await() const& noexcept {
        struct awaitable : awaitable_base {
            explicit awaitable(std::coroutine_handle<promise_type> handle) noexcept : awaitable_base(handle) {}

            decltype(auto) await_resume() {
                if (!this->continuation) [[unlikely]] {
                    throw std::system_error(coro_errors::broken_promise);
                }

                awaitable_base::check_token();

                return this->continuation.promise().result();
            }
        };

        return awaitable{continuation_};
    }

    auto operator co_await() const&& noexcept {
        struct awaitable : awaitable_base {
            explicit awaitable(std::coroutine_handle<promise_type> handle) noexcept : awaitable_base(handle) {}

            decltype(auto) await_resume() {
                if (!this->continuation) [[unlikely]] {
                    throw std::system_error(coro_errors::broken_promise);
                }

                awaitable_base::check_token();

                return std::move(this->continuation.promise()).result();
            }
        };

        return awaitable{continuation_};
    }

    [[nodiscard]] auto when_ready() const noexcept {
        struct awaitable : awaitable_base {
            explicit awaitable(std::coroutine_handle<promise_type> handle) noexcept : awaitable_base(handle) {}

            void await_resume() const { awaitable_base::check_token(); }
        };

        return awaitable{continuation_};
    }

  private:
    std::coroutine_handle<promise_type> continuation_;
};

template <typename T>
task<T> task_promise<T>::get_return_object() noexcept {
    return task<T>{std::coroutine_handle<task_promise>::from_promise(*this)};
}

inline task<void> task_promise<void>::get_return_object() noexcept {
    return task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
}

template <typename T>
task<T&> task_promise<T&>::get_return_object() noexcept {
    return task<T&>{std::coroutine_handle<task_promise>::from_promise(*this)};
}

}  // namespace simple
