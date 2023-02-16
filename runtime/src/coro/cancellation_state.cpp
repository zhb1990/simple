#include "cancellation_state.h"

#include <simple/coro/cancellation_registration.h>

namespace simple {

cancellation_state::cancellation_state() : state_(state::none), registrations_(nullptr) {}

void cancellation_state::register_callback(cancellation_registration* registration) {
    if (!can_be_cancelled()) return;

    if (registration->prev_ != nullptr || registration == registrations_) {
        return;
    }

    registration->next_ = registrations_;
    registration->prev_ = nullptr;

    if (registrations_) {
        registrations_->prev_ = registration;
    }

    registrations_ = registration;
}

void cancellation_state::deregister_callback(cancellation_registration* registration) noexcept {
    if (registrations_ == registration) {
        registrations_ = registration->next_;
    }
    if (registration->prev_) {
        registration->prev_->next_ = registration->next_;
    }
    if (registration->next_) {
        registration->next_->prev_ = registration->prev_;
    }
    registration->next_ = nullptr;
    registration->prev_ = nullptr;
}

bool cancellation_state::can_be_cancelled() const noexcept { return state_ == state::none; }

bool cancellation_state::is_cancellation_requested() const noexcept { return state_ == state::cancelled; }

void cancellation_state::request_cancellation() {
    if (is_cancellation_requested()) return;
    state_ = state::cancelled;
    while (registrations_) {
        auto* registration = registrations_;
        registrations_ = registrations_->next_;
        registration->next_ = nullptr;
        registration->prev_ = nullptr;
        registration->state_.reset();
        registration->callback_();
    }
}

}  // namespace simple
