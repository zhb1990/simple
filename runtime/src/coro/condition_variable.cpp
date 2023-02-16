#include <simple/coro/condition_variable.h>
#include <simple/error.h>

void simple::condition_variable_state::add_awaiter(simple::condition_variable_awaiter* awaiter) noexcept {
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

void simple::condition_variable_state::remove_awaiter(simple::condition_variable_awaiter* awaiter) noexcept {
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

void simple::condition_variable_state::notify_one() noexcept {
    if (!header_) {
        return;
    }

    auto* awaiter = header_;
    remove_awaiter(awaiter);
    scheduler::instance().wake_up_coroutine(awaiter->handle_);
}

void simple::condition_variable_state::notify_all() noexcept {
    auto* awaiter = header_;
    while (awaiter) {
        auto* current = awaiter;
        awaiter = current->next_;
        current->next_ = nullptr;
        current->prev_ = nullptr;
        scheduler::instance().wake_up_coroutine(current->handle_);
    }

    header_ = nullptr;
    tail_ = nullptr;
}

bool simple::condition_variable_state::has_awaiter(simple::condition_variable_awaiter* awaiter) const noexcept {
    return awaiter->next_ || awaiter->prev_ || header_ == awaiter;
}

simple::condition_variable_awaiter::condition_variable_awaiter(simple::condition_variable_awaiter&& other) noexcept {
    state_ = other.state_;
}

void simple::condition_variable_awaiter::await_resume() {
    registration_.reset();
    if (token_.is_cancellation_requested()) {
        throw std::system_error(coro_errors::canceled);
    }
}

simple::condition_variable::condition_variable() : state_(std::make_shared<condition_variable_state>()) {}

simple::condition_variable_awaiter simple::condition_variable::wait() const noexcept {
    condition_variable_awaiter waiter;
    waiter.state_ = state_;
    return waiter;
}

void simple::condition_variable::notify_one() const noexcept { state_->notify_one(); }

void simple::condition_variable::notify_all() const noexcept { state_->notify_all(); }
