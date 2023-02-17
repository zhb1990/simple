#include <simple/coro/cancellation_source.h>
#include <simple/coro/cancellation_token.h>

#include "cancellation_state.h"

namespace simple {

cancellation_token::cancellation_token(const cancellation_source& source) : state_(source.state_) {}

bool cancellation_token::can_be_cancelled() const noexcept { return state_ && state_->can_be_cancelled(); }

bool cancellation_token::is_cancellation_requested() const noexcept { return state_ && state_->is_cancellation_requested(); }

}  // namespace simple
