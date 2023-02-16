#include <simple/coro/thread_pool.h>

#if defined(_WIN32)
#include <Windows.h>
#elif !defined(macintosh) && !defined(Macintosh) && !(defined(__APPLE__) && defined(__MACH__))
#include <sched.h>
#include <unistd.h>
#endif

namespace simple {

thread_pool& thread_pool::instance() {
    static thread_pool pool;
    return pool;
}

void thread_pool::start(size_t num) {
    if (!threads_.empty()) {
        return;
    }

    size_t real;
    if (num == 0) {
        real = std::thread::hardware_concurrency();
        if (real > 1) {
            --real;
        }
    } else {
        real = num;
    }

    for (size_t i = 0; i < real; ++i) {
#if !defined(_WIN32) && !defined(macintosh) && !defined(Macintosh) && !(defined(__APPLE__) && defined(__MACH__))
        threads_.emplace_back([this, num, i](const std::stop_token& token) {
            if (num == 0) {
                cpu_set_t mask;
                CPU_ZERO(&mask);
                CPU_SET(i + 1, &mask);
                sched_setaffinity(0, sizeof(mask), &mask);
            }
            return run(token);
        });
#else
        threads_.emplace_back([this](const std::stop_token& token) { return run(token); });
#endif
    }

#if defined(_WIN32)
    if (num == 0) {
        for (size_t i = 0; i < real; ++i) {
            constexpr DWORD_PTR def = 2;
            SetThreadAffinityMask(threads_[i].native_handle(), def << i);
        }
    }
#endif
}

void thread_pool::stop() {
    for (auto& t : threads_) {
        t.request_stop();
    }
}

void thread_pool::join() {
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }

    threads_.clear();
}

void thread_pool::post(std::function<void()>&& func) {
    std::unique_lock lock(mutex_);
    queue_.emplace_back(std::move(func));
    cv_.notify_one();
}

void thread_pool::run(const std::stop_token& token) {
    std::function<void()> func;
    while (!token.stop_requested()) {
        {
            std::unique_lock lock(mutex_);
            if (!cv_.wait(lock, token, [this]() { return !queue_.empty(); })) {
                continue;
            }

            func = std::move(queue_.front());
            queue_.pop_front();
        }

        func();
    }
}

}  // namespace simple
