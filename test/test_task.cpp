#include <gtest/gtest.h>
#include <simple/coro/async_session.h>
#include <simple/coro/cancellation_source.h>
#include <simple/coro/thread_pool.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/coro/task_operators.hpp>
#include <simple/coro/co_start.hpp>
#include <simple/coro/parallel_task.hpp>
#include <simple/coro/sync_wait.hpp>
#include <simple/coro/task.hpp>
#include <stdexcept>
#include <string>
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
        const auto ec = make_error_code(simple::coro_errors::canceled);
        EXPECT_EQ(e.what(), ec.message());
    }

    EXPECT_EQ(a, 10);
}

TEST(task, async_session) {
    int a = 10;
    sync_wait([&a]() -> simple::task<> {
        simple::async_session_awaiter<int> awaiter;
        simple::thread_pool::instance().post([session = awaiter.get_async_session()] { session.set_result(111); });

        a = co_await awaiter;
    }());

    EXPECT_EQ(a, 111);
}

TEST(task, parallel_task) {
    auto task1 = []() -> simple::task<int> { co_return 101; };
    auto task2 = []() -> simple::task<std::string> { co_return "hello"; };

    auto [ret1, ret2] = sync_wait(wait_parallel_task_ready(simple::parallel_task_type::wait_all, task1(), task2()));
    EXPECT_EQ(ret1.result(), 101);
    EXPECT_EQ(ret2.result(), "hello");
}

TEST(task, task_operators) {
    auto task1 = []() -> simple::task<int> {
        co_await simple::sleep_for(100ms);
        co_return 101;
    };

    auto task2 = []() -> simple::task<std::string> {
        co_await simple::sleep_for(200ms);
        co_return "hello";
    };

    auto [ret1, ret2] = sync_wait(task1() && task2());
    EXPECT_EQ(ret1, 101);
    EXPECT_EQ(ret2, "hello");

    auto ret3 = sync_wait(task1() || task2());
    EXPECT_EQ(ret3.index(), 0);
    EXPECT_EQ(std::get<0>(ret3), 101);
}
