#include <simple/coro/timed_awaiter.h>
#include <simple/error.h>

namespace simple {

timed_awaiter::timed_awaiter(time_point point) { node.point = point; }

timed_awaiter::timed_awaiter(duration dur) : timed_awaiter(clock::now() + dur) {}

timed_awaiter::timed_awaiter(timed_awaiter&& other) noexcept { node.point = other.node.point; }

void timed_awaiter::await_resume() {
    registration_.reset();
    if (token_.is_cancellation_requested()) {
        throw std::system_error(coro_errors::canceled);
    }
}

void timed_awaiter::wake_up() const { handle_.resume(); }

}  // namespace simple
