#include <simple/coro/cancellation_source.h>

#include "cancellation_state.h"

namespace simple {

cancellation_source::cancellation_source() : state_(std::make_shared<cancellation_state>()) {}

cancellation_token cancellation_source::token() const noexcept { return cancellation_token{*this}; }

void cancellation_source::request_cancellation() const {
    if (state_) {
        state_->request_cancellation();
    }
}

bool cancellation_source::can_be_cancelled() const noexcept { return state_ && state_->can_be_cancelled(); }

bool cancellation_source::is_cancellation_requested() const noexcept { return state_ && state_->is_cancellation_requested(); }

}  // namespace simple
