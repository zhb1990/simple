#pragma once

#include <simple/config.h>
#include <simple/containers/time_queue.h>

#include <condition_variable>
#include <coroutine>
#include <deque>
#include <functional>
#include <thread>
#include <unordered_map>

namespace simple {

class scheduler {
    scheduler() = default;

  public:
    SIMPLE_NON_COPYABLE(scheduler)
    ~scheduler() noexcept = default;

    SIMPLE_API static scheduler& instance();

    SIMPLE_API void post(std::function<void()> func);

    SIMPLE_API void post_immediate(std::function<void()> func);

    // 启动线程
    SIMPLE_API void start();

    SIMPLE_API void stop();

    SIMPLE_API void join();

    auto& get_timer_queue() { return timer_queue_; }

    SIMPLE_API void wake_up_coroutine(std::coroutine_handle<> handle) noexcept;

    static auto* current_scheduler() noexcept { return current_scheduler_; }

  private:
    void run(const std::stop_token& token);

    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<std::function<void()>> queue_;
    timer_queue timer_queue_;

    std::jthread thread_;

    std::deque<std::coroutine_handle<>> wake_up_coroutine_;

    inline static thread_local scheduler* current_scheduler_ = nullptr;
};

}  // namespace simple
