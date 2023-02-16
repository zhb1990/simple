#pragma once

#include <simple/config.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <thread>

namespace simple {

// 线程池
class thread_pool {
    thread_pool() = default;

  public:
    SIMPLE_NON_COPYABLE(thread_pool)

    ~thread_pool() noexcept = default;

    SIMPLE_API static thread_pool& instance();

    // 启动线程池, 参数为线程数量
    SIMPLE_API void start(size_t num);

    SIMPLE_API void stop();

    SIMPLE_API void join();

    SIMPLE_API void post(std::function<void()>&& func);

  private:
    void run(const std::stop_token& token);

    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<std::function<void()>> queue_;
    std::vector<std::jthread> threads_;
};

}  // namespace simple
