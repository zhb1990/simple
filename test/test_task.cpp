#include <gtest/gtest.h>
#include <simple/coro/cancellation_source.h>
#include <simple/coro/timed_awaiter.h>

#include <simple/coro/co_start.hpp>
#include <simple/coro/sync_wait.hpp>
#include <simple/coro/task.hpp>
#include <stdexcept>
#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

TEST(task, sleep) {
    int a = 10;
    sync_wait([&a]() -> simple::task<> {
        co_await simple::sleep_for(100ms);
        a = 100;
    }());

    EXPECT_EQ(a, 100);
}

TEST(task, sync_wait_result) {
    const auto ret = sync_wait([]() -> simple::task<int> {
        co_await simple::sleep_for(100ms);
        co_return 10;
    }());

    EXPECT_EQ(ret, 10);
}

TEST(task, sync_wait_exception) {
    try {
        sync_wait([]() -> simple::task<> { throw std::logic_error("sync_wait_exception"); }());
    } catch (std::exception &e) {
        EXPECT_EQ(e.what(), "sync_wait_exception"sv);
    }
}

TEST(task, cancellation) {
    simple::cancellation_source source;
    int a = 10;
    simple::co_start([&source]() -> simple::task<> {
        co_await simple::sleep_for(100ms);
        source.request_cancellation();
    });

    try {
        sync_wait([&a, &source]() -> simple::task<> {
            co_await simple::set_cancellation_token_awaiter{source.token()};
            co_await simple::sleep_for(10s);
            a = 100;
        }());
    } catch (std::exception &e) {
        auto ec = make_error_code(simple::coro_errors::canceled);
        EXPECT_EQ(e.what(), ec.message());
    }

    EXPECT_EQ(a, 10);
}
