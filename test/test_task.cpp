#include <gtest/gtest.h>
#include <simple/coro/timed_awaiter.h>

#include <simple/coro/sync_wait.hpp>
#include <simple/coro/task.hpp>

TEST(task, sleep_1s) {
    sync_wait([]() -> simple::task<> {
        using namespace std::chrono_literals;
        co_await simple::sleep_for(1s);
    }());
}

TEST(task, sync_wait_result) {
    const auto ret = sync_wait([]() -> simple::task<int> {
        using namespace std::chrono_literals;
        co_await simple::sleep_for(1s);
        co_return 10;
    }());

    EXPECT_EQ(ret, 10);
}


