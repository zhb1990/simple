#pragma once
#include <simple/config.h>
#include <simple/coro/cancellation_registration.h>
#include <simple/coro/cancellation_token.h>
#include <simple/error.h>

#include <cassert>
#include <coroutine>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace simple {

class async_system {
    async_system() = default;

  public:
    ~async_system() noexcept = default;

    DS_NON_COPYABLE(async_system)

    DS_API static async_system& instance();

    DS_API uint64_t create_session() noexcept;

    DS_API void insert_session(uint64_t session, std::coroutine_handle<> handle);

    // 唤醒对应的协程
    DS_API void wake_up_session(uint64_t session) noexcept;

  private:
    uint64_t session_{0};
    std::unordered_map<uint64_t, std::coroutine_handle<>> wait_map_;
};

template <typename T>
class async_value {
  public:
    template <typename V>
    requires std::convertible_to<V&&, T>
    void set_value(V&& value) noexcept(std::is_nothrow_constructible_v<T, V&&>) {
        value_.template emplace<T>(std::forward<V>(value));
    }

    void set_exception(const std::exception_ptr& e) noexcept { value_.template emplace<std::exception_ptr>(e); }

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
class async_value<void> {
  public:
    // ReSharper disable once CppMemberFunctionMayBeStatic
    void set_value() const noexcept {}

    void set_exception(const std::exception_ptr& e) noexcept { exception_ = e; }

    void result() const {
        if (exception_) [[unlikely]] {
            std::rethrow_exception(exception_);
        }
    }

  private:
    std::exception_ptr exception_;
};

template <typename T>
class async_value<T&> {
  public:
    void set_value(T& value) noexcept { value_.template emplace<T*>(std::addressof(value)); }

    void set_exception(const std::exception_ptr& e) noexcept { value_.template emplace<std::exception_ptr>(e); }

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

struct async_session_base {
    uint64_t session{0};

    void wake_up() const { async_system::instance().wake_up_session(session); }
};

template <typename Result>
struct async_session : async_session_base {
    std::weak_ptr<async_value<Result>> result;

    template <typename T>
    void set_result(T&& t) const {
        if (const auto shared = result.lock()) {
            shared->set_value(std::forward<T>(t));
            wake_up();
        }
    }

    void set_exception(const std::exception_ptr& e) const {
        if (const auto shared = result.lock()) {
            shared->set_exception(e);
            wake_up();
        }
    }
};

template <>
struct async_session<void> : async_session_base {
    std::weak_ptr<async_value<void>> result;

    void set_result() const {
        if (const auto shared = result.lock()) {
            shared->set_value();
            wake_up();
        }
    }

    void set_exception(const std::exception_ptr& e) const {
        if (const auto shared = result.lock()) {
            shared->set_exception(e);
            wake_up();
        }
    }
};

template <typename Result>
class async_session_awaiter {
  public:
    async_session_awaiter() {
        session_ = async_system::instance().create_session();
        result_ = std::make_shared<async_value<Result>>();
    }

    async_session_awaiter(const async_session_awaiter&) = delete;

    async_session_awaiter(async_session_awaiter&& other) noexcept
        : session_(other.session_), result_(std::move(other.result_)) {
        other.session_ = 0;
    }

    ~async_session_awaiter() noexcept = default;

    async_session_awaiter& operator=(async_session_awaiter&) = delete;

    async_session_awaiter& operator=(async_session_awaiter&&) noexcept = delete;

    [[nodiscard]] async_session<Result> get_async_session() const noexcept { return {{session_}, result_}; }

    struct awaitable_base {
        explicit awaitable_base(async_session_awaiter* ptr) noexcept : awaiter(ptr) {}

        // ReSharper disable once CppMemberFunctionMayBeStatic
        [[nodiscard]] bool await_ready() const noexcept { return !awaiter->result_; }

        template <typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> handle) {
            awaiter->handle_ = handle;
            awaiter->token_ = handle.promise().get_cancellation_token();

            if (awaiter->token_.is_cancellation_requested()) {
                return false;
            }

            auto& system = async_system::instance();
            if (awaiter->token_.can_be_cancelled()) {
                awaiter->registration_.emplace(awaiter->token_, [this, &system] { system.wake_up_session(awaiter->session_); });
            }

            system.insert_session(awaiter->session_, handle);

            return true;
        }

        void check_resume() const {
            awaiter->registration_.reset();
            if (awaiter->token_.is_cancellation_requested()) [[unlikely]] {
                throw std::system_error(coro_errors::canceled);
            }

            if (!awaiter->result_) [[unlikely]] {
                throw std::system_error(coro_errors::invalid_action);
            }
        }

        async_session_awaiter* awaiter;
    };

    auto operator co_await() & noexcept {
        struct awaitable : awaitable_base {
            explicit awaitable(async_session_awaiter* ptr) noexcept : awaitable_base(ptr) {}

            decltype(auto) await_resume() {
                this->check_resume();
                return this->awaiter->result_->result();
            }
        };

        return awaitable{this};
    }

    auto operator co_await() && noexcept {
        struct awaitable : awaitable_base {
            explicit awaitable(async_session_awaiter* ptr) noexcept : awaitable_base(ptr) {}

            decltype(auto) await_resume() {
                this->check_resume();
                return std::move(*this->awaiter->result_).result();
            }
        };

        return awaitable{this};
    }

  private:
    uint64_t session_;
    std::shared_ptr<async_value<Result>> result_;
    std::coroutine_handle<> handle_;
    cancellation_token token_;
    std::optional<cancellation_registration> registration_;
};

}  // namespace simple
