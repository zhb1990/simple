#include "proto_rpc.h"

#include <simple/error.h>
#include <simple/utils/os.h>

#include <ctime>
#include <stdexcept>

void rpc_awaiter_base::check_resume() {
    registration_.reset();
    if (token_.is_cancellation_requested()) [[unlikely]] {
        throw std::system_error(simple::coro_errors::canceled);
    }

    if (exception_) [[unlikely]] {
        std::rethrow_exception(exception_);
    }
}

void rpc_awaiter_base::parse_message(std::string_view data) {
    try {
        if (!message_ptr_->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            throw std::logic_error("rpc parse message fail");
        }
    } catch (...) {
        exception_ = std::current_exception();
    }
}

uint64_t rpc_system::create_session() noexcept {
    uint64_t ti = time(nullptr);
    if (ti < time_) {
        ti = time_;
    } else if (ti > time_) {
        time_ = ti;
        sequence_ = 0;
    }

    uint64_t session = ti << 32;
    session |= (static_cast<uint64_t>(simple::os::pid()) & 0x7Full) << 25;
    return session | (++sequence_ & 0x1FFFFFF);
}

void rpc_system::insert_session(uint64_t session, rpc_awaiter_base* awaiter) { wait_map_.emplace(session, awaiter); }

void rpc_system::wake_up_session(uint64_t session) noexcept {
    if (const auto it = wait_map_.find(session); it != wait_map_.end()) {
        const auto* awaiter = it->second;
        wait_map_.erase(it);
        simple::scheduler::instance().wake_up_coroutine(awaiter->handle_);
    }
}

bool rpc_system::wake_up_session(uint64_t session, std::string_view data) noexcept {
    if (const auto it = wait_map_.find(session); it != wait_map_.end()) {
        auto* awaiter = it->second;
        wait_map_.erase(it);
        awaiter->parse_message(data);
        simple::scheduler::instance().wake_up_coroutine(awaiter->handle_);
        return true;
    }

    return false;
}
