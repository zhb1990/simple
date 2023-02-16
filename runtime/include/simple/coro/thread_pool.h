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
    DS_NON_COPYABLE(thread_pool)

    ~thread_pool() noexcept = default;

    DS_API static thread_pool& instance();

    // 启动线程池, 参数为线程数量
    DS_API void start(size_t num);

    DS_API void stop();

    DS_API void join();

    DS_API void post(std::function<void()>&& func);

  private:
    void run(const std::stop_token& token);

    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<std::function<void()>> queue_;
    std::vector<std::jthread> threads_;
};

}  // namespace simple
