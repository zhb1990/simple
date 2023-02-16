#include <simple/coro/async_session.h>
#include <simple/coro/scheduler.h>

namespace simple {

async_system& async_system::instance() {
    static async_system ins;
    return ins;
}

uint64_t async_system::create_session() noexcept {
    auto session = ++session_;
    if (session == 0) {
        session = ++session_;
    }

    return session;
}

void async_system::insert_session(uint64_t session, std::coroutine_handle<> handle) { wait_map_.emplace(session, handle); }

void async_system::wake_up_session(uint64_t session) noexcept {
    auto& scheduler = scheduler::instance();
    if (scheduler::current_scheduler() == &scheduler) {
        if (const auto it = wait_map_.find(session); it != wait_map_.end()) {
            const auto handle = it->second;
            wait_map_.erase(it);
            scheduler.wake_up_coroutine(handle);
        }

        return;
    }

    scheduler.post([session, this]() {
        if (const auto it = wait_map_.find(session); it != wait_map_.end()) {
            const auto handle = it->second;
            wait_map_.erase(it);
            handle.resume();
        }
    });
}
}  // namespace simple
