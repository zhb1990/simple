#include <simple/coro/cancellation_registration.h>
#include <simple/coro/cancellation_token.h>

#include "cancellation_state.h"

namespace simple {

cancellation_registration::cancellation_registration(const cancellation_token& token, std::function<void()>&& callback)
    : next_(nullptr), prev_(nullptr), callback_(std::move(callback)) {
    if (token.state_ && token.state_->can_be_cancelled()) {
        state_ = token.state_;
        state_->register_callback(this);
    }
}

cancellation_registration::~cancellation_registration() noexcept {
    if (state_) {
        state_->deregister_callback(this);
    }
}

}  // namespace simple
