#include <simple/coro/scheduler.h>
#include <simple/coro/timed_awaiter.h>

namespace simple {

scheduler& scheduler::instance() {
    static scheduler ins;
    return ins;
}

void scheduler::post(std::function<void()> func) {
    std::unique_lock lock(mutex_);
    queue_.emplace_back(std::move(func));
    cv_.notify_one();
}

void scheduler::post_immediate(std::function<void()> func) {
    if (current_scheduler_ == this) {
        return func();
    }

    return post(std::move(func));
}

void scheduler::start() {
    thread_ = std::jthread([this](const std::stop_token& token) { return run(token); });
}

void scheduler::stop() { thread_.request_stop(); }

void scheduler::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void scheduler::wake_up_coroutine(std::coroutine_handle<> handle) noexcept {
    if (current_scheduler_ == this) {
        wake_up_coroutine_.emplace_back(handle);
        return;
    }

    post([handle]() { handle.resume(); });
}

void scheduler::run(const std::stop_token& token) {
    current_scheduler_ = this;
    auto now = timer_queue::clock::now();
    auto dur = timer_queue_.wait_duration(now);
    while (!token.stop_requested()) {
        {
            // 处理消息
            std::unique_lock lock(mutex_);
            if (cv_.wait_for(lock, token, dur, [this]() { return !queue_.empty(); })) {
                std::deque<std::function<void()>> functions = std::move(queue_);
                lock.unlock();
                for (const auto& func : functions) {
                    func();
                }
            }
        }

        // 处理定时器
        now = timer_queue::clock::now();
        const auto nodes = timer_queue_.get_ready_timers(now);
        for (auto* ptr : nodes) {
            if (const auto* timed = dynamic_cast<timed_awaiter*>(ptr)) {
                timed->wake_up();
            }
        }

        // 处理要恢复的协程
        const auto wake_size = wake_up_coroutine_.size();
        while (!wake_up_coroutine_.empty()) {
            auto handle = wake_up_coroutine_.front();
            wake_up_coroutine_.pop_front();
            handle.resume();
        }

        // 根据定时器重新设置等待时间
        if (!nodes.empty() || wake_size > 0) {
            now = timer_queue::clock::now();
        }
        dur = timer_queue_.wait_duration(now);
    }
}

}  // namespace simple
