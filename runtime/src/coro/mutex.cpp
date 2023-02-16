#include <simple/coro/mutex.h>
#include <simple/error.h>

namespace simple {

bool mutex_state::try_lock(mutex_awaiter* awaiter) noexcept {
    if (awaiter->handle_ == current_) {
        // 当前协程已经加锁了
        return true;
    }

    if (!locked()) {
        current_ = awaiter->handle_;
        return true;
    }

    add_awaiter(awaiter);
    return false;
}

void mutex_state::unlock() noexcept {
    if (!locked()) {
        return;
    }

    if (header_) {
        mutex_awaiter* awaiter = header_;
        remove_awaiter(awaiter);
        current_ = awaiter->handle_;
        scheduler::instance().wake_up_coroutine(current_);
    } else {
        current_ = nullptr;
    }
}

void mutex_state::add_awaiter(mutex_awaiter* awaiter) noexcept {
    if (awaiter->next_ != nullptr || awaiter == header_) {
        return;
    }

    if (awaiter->prev_ != nullptr || awaiter == tail_) {
        return;
    }

    awaiter->next_ = nullptr;
    awaiter->prev_ = tail_;

    if (tail_) {
        tail_->next_ = awaiter;
    }

    tail_ = awaiter;

    if (!header_) {
        header_ = awaiter;
    }
}

void mutex_state::remove_awaiter(mutex_awaiter* awaiter) noexcept {
    if (header_ == awaiter) {
        header_ = awaiter->next_;
    }

    if (tail_ == awaiter) {
        tail_ = awaiter->prev_;
    }

    if (awaiter->prev_) {
        awaiter->prev_->next_ = awaiter->next_;
    }
    if (awaiter->next_) {
        awaiter->next_->prev_ = awaiter->prev_;
    }
    awaiter->next_ = nullptr;
    awaiter->prev_ = nullptr;
}

std::coroutine_handle<> mutex_state::current() const noexcept { return current_; }

mutex_awaiter::mutex_awaiter(mutex_awaiter&& other) noexcept { state_ = other.state_; }

void mutex_awaiter::await_resume() {
    registration_.reset();
    if (token_.is_cancellation_requested()) {
        throw std::system_error(coro_errors::canceled);
    }
}

mutex::mutex() : state_(std::make_shared<mutex_state>()) {}

mutex_awaiter mutex::lock() const noexcept {
    mutex_awaiter waiter;
    waiter.state_ = state_;
    return waiter;
}

// bool mutex::try_lock() const {
//     return true;
// }

void mutex::unlock() const noexcept { return state_->unlock(); }

}  // namespace simple
