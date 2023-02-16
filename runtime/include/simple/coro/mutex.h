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

class mutex;
class mutex_awaiter;

class mutex_state {
  public:
    mutex_state() = default;

    SIMPLE_NON_COPYABLE(mutex_state)

    ~mutex_state() noexcept = default;

    [[nodiscard]] auto locked() const noexcept { return current_ != nullptr; }

    SIMPLE_API bool try_lock(mutex_awaiter* awaiter) noexcept;

    void unlock() noexcept;

    void add_awaiter(mutex_awaiter* awaiter) noexcept;

    SIMPLE_API void remove_awaiter(mutex_awaiter* awaiter) noexcept;

    [[nodiscard]] SIMPLE_API std::coroutine_handle<> current() const noexcept;

  private:
    std::coroutine_handle<> current_;
    mutex_awaiter* header_{nullptr};
    mutex_awaiter* tail_{nullptr};
};

class mutex_awaiter {
    mutex_awaiter() = default;

  public:
    SIMPLE_API mutex_awaiter(mutex_awaiter&& other) noexcept;

    mutex_awaiter(const mutex_awaiter&) = delete;

    ~mutex_awaiter() noexcept = default;

    mutex_awaiter& operator=(mutex_awaiter&) = delete;

    mutex_awaiter& operator=(mutex_awaiter&&) noexcept = delete;

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        handle_ = handle;
        token_ = handle.promise().get_cancellation_token();

        if (token_.is_cancellation_requested() || (!state_) || state_->try_lock(this)) {
            return false;
        }

        if (token_.can_be_cancelled()) {
            auto& scheduler = scheduler::instance();
            registration_.emplace(token_, [this, &scheduler]() {
                if (state_->current() != handle_) {
                    state_->remove_awaiter(this);
                    scheduler.wake_up_coroutine(handle_);
                }
            });
        }

        return true;
    }

    SIMPLE_API void await_resume();

  protected:
    friend class mutex;
    friend class mutex_state;

    std::shared_ptr<mutex_state> state_;
    cancellation_token token_;
    std::optional<cancellation_registration> registration_;
    std::coroutine_handle<> handle_;
    mutex_awaiter* next_{nullptr};
    mutex_awaiter* prev_{nullptr};
};

// 注意不能重复加锁
class mutex {
  public:
    SIMPLE_API mutex();

    SIMPLE_COPYABLE_DEFAULT(mutex)

    ~mutex() noexcept = default;

    [[nodiscard]] SIMPLE_API mutex_awaiter lock() const noexcept;

    // SIMPLE_API bool try_lock() const;

    SIMPLE_API void unlock() const noexcept;

  private:
    std::shared_ptr<mutex_state> state_;
};

class scoped_lock {
    explicit scoped_lock(mutex& mtx) : mtx_(&mtx) {}

  public:
    scoped_lock(const scoped_lock&) = delete;

    scoped_lock(scoped_lock&& other) noexcept : mtx_(other.mtx_) { other.mtx_ = nullptr; }

    ~scoped_lock() noexcept {
        if (mtx_) {
            mtx_->unlock();
        }
    }

    scoped_lock& operator=(scoped_lock&) = delete;

    scoped_lock& operator=(scoped_lock&&) noexcept = delete;

    friend task<scoped_lock> make_scoped_lock(mutex& mtx);

  private:
    mutex* mtx_;
};

inline task<scoped_lock> make_scoped_lock(mutex& mtx) {
    co_await mtx.lock();
    co_return scoped_lock{mtx};
}

class unique_lock {
    friend task<unique_lock> make_unique_lock(mutex& mtx);

    explicit unique_lock(mutex& mtx) : mtx_(&mtx), locked_(true) {}

  public:
    unique_lock() = default;

    unique_lock(const unique_lock&) = delete;

    unique_lock(unique_lock&& other) noexcept : mtx_(other.mtx_), locked_(other.locked_) {
        other.mtx_ = nullptr;
        other.locked_ = false;
    }

    ~unique_lock() noexcept {
        if (locked_) {
            mtx_->unlock();
        }
    }

    unique_lock& operator=(unique_lock&) = delete;

    unique_lock& operator=(unique_lock&& other) noexcept {
        if (std::addressof(other) != this) {
            if (locked_) {
                mtx_->unlock();
            }

            locked_ = other.locked_;
            mtx_ = other.mtx_;

            other.mtx_ = nullptr;
            other.locked_ = false;
        }
        return *this;
    }

    task<> lock() {
        if (locked_) {
            co_return;
        }

        co_await mtx_->lock();
        locked_ = true;
    }

    void unlock() noexcept {
        if (!locked_) {
            return;
        }

        locked_ = false;
        mtx_->unlock();
    }

  private:
    mutex* mtx_{nullptr};
    bool locked_{false};
};

inline task<unique_lock> make_unique_lock(mutex& mtx) {
    co_await mtx.lock();
    co_return unique_lock{mtx};
}

}  // namespace simple
