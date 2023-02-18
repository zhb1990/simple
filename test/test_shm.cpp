#include <gtest/gtest.h>
#include <simple/shm/shm_channel.h>

#include <simple/coro/sync_wait.hpp>
#include <simple/coro/task_operators.hpp>
#include <string_view>

using namespace std::string_view_literals;

TEST(shm, buffer) {
    simple::shm_buffer sb2("111", 15);
    sb2.write("hello world", 11);
    char pc1[15];
    auto l1 = sb2.read(pc1, 15);
    pc1[l1] = 0;
    EXPECT_EQ(pc1, "hello world"sv);

    sb2.write("hello world", 11);
    char pc2[15];
    auto l2 = sb2.read(pc2, 15);
    pc2[l2] = 0;
    EXPECT_EQ(pc2, "hello world"sv);
}

TEST(shm, channel) {
    simple::memory_buffer recv_data;
    const auto send_data = "hello world"sv;
    constexpr size_t channel_size = 1024;

    simple::shm_channel channel1("1", "2", channel_size);
    auto task1 = [&]() -> simple::task<> {
        co_await channel1.write(send_data.data(), static_cast<uint32_t>(send_data.size()));
    };

    simple::shm_channel channel2("2", "1", channel_size);
    auto task2 = [&]() -> simple::task<> { co_await channel2.read(recv_data); };

    sync_wait(task1() && task2());

    EXPECT_EQ(std::string_view(recv_data), send_data);
}
