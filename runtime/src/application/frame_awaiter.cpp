#include <simple/application/frame_awaiter.h>
#include <simple/error.h>

namespace simple {

frame_awaiter::frame_awaiter(uint64_t frame) : frame_(frame) {}

frame_awaiter::frame_awaiter(frame_awaiter&& other) noexcept : frame_(other.frame_) { other.frame_ = 0; }

void frame_awaiter::await_resume() {
    registration_.reset();
    if (token_.is_cancellation_requested()) {
        throw std::system_error(coro_errors::canceled);
    }
}

frame_awaiter skip_frame(uint64_t skip) {
    if (skip == 0) {
        skip = 1;
    }
    return frame_awaiter{application::instance().frame() + skip};
}

}  // namespace simple
