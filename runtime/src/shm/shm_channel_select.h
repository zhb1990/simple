#pragma once

#include <simple/coro/async_session.h>
#include <simple/shm/shm_channel.h>

#include <condition_variable>
#include <deque>
#include <simple/coro/task.hpp>
#include <thread>
#include <vector>

namespace simple {

// 换成单独的线程去检查共享内存通道的状态
class shm_channel_select {
    shm_channel_select() = default;

  public:
    SIMPLE_NON_COPYABLE(shm_channel_select)

    ~shm_channel_select() noexcept = default;

    static shm_channel_select& instance();

    void start();

    void join();

    void stop();

    simple::task<> wait(shm_channel* channel, bool is_read_or_write, size_t size);

  private:
    struct select_data {
        shm_channel* channel{nullptr};
        async_session<void> session;
        bool is_read_or_write{true};
        bool done{false};
        size_t size{0};
    };

    static bool is_ready(const select_data& data);

    void run(const std::stop_token& token);

    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<select_data> queue_;
    std::jthread thread_;

    std::vector<select_data> select_;
};

}  // namespace simple
