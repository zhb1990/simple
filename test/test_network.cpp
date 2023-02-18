#include <gtest/gtest.h>
#include <simple/coro/network.h>
#include <simple/utils/time.h>

#include <simple/coro/sync_wait.hpp>
#include <simple/coro/task_operators.hpp>
#include <string_view>

TEST(network, connect_disconnect_tcp) {
    auto server = [&]() -> simple::task<> {
        auto& network = simple::network::instance();
        const auto listen_id = co_await network.tcp_listen("", 10034, true);
        co_await network.accept(listen_id);
        network.close(listen_id);
    };

    auto client = [&]() -> simple::task<> {
        auto& network = simple::network::instance();
        const auto client_id = co_await network.tcp_connect("localhost", "10034", std::chrono::seconds(10));
        network.close(client_id);
    };

    sync_wait(server() && client());
}

TEST(network, send_recv_tcp) {
    simple::memory_buffer recv_data;
    std::string_view send_data{"hello"};
    auto t1 = simple::get_timestamp_millis();
    auto server = [&]() -> simple::task<> {
        auto& network = simple::network::instance();
        const auto listen_id = co_await network.tcp_listen("", 10034, true);
        const auto session = co_await network.accept(listen_id);

        recv_data.make_sure_writable(send_data.size());
        const auto len = co_await network.read_size(session, recv_data.begin_write(), send_data.size());
        recv_data.written(len);

        network.close(session);
        network.close(listen_id);
    };

    auto client = [&]() -> simple::task<> {
        auto& network = simple::network::instance();
        const auto client_id = co_await network.tcp_connect("localhost", "10034", std::chrono::seconds(10));
        network.write(client_id, std::make_shared<simple::memory_buffer>(send_data.data(), send_data.size()));
    };

    sync_wait(server() && client());
    EXPECT_EQ(send_data, std::string_view(recv_data));
}

TEST(network, send_recv_kcp) {
    simple::memory_buffer recv_data;
    std::string_view send_data{"hello"};
    auto t1 = simple::get_timestamp_millis();
    auto server = [&]() -> simple::task<> {
        auto& network = simple::network::instance();
        const auto listen_id = co_await network.kcp_listen("", 10034, true);
        const auto session = co_await network.accept(listen_id);

        recv_data.make_sure_writable(send_data.size());
        const auto len = co_await network.read_size(session, recv_data.begin_write(), send_data.size());
        recv_data.written(len);

        network.close(session);
        network.close(listen_id);
    };

    auto client = [&]() -> simple::task<> {
        auto& network = simple::network::instance();
        const auto client_id = co_await network.kcp_connect("localhost", "10034", std::chrono::seconds(10));
        network.write(client_id, std::make_shared<simple::memory_buffer>(send_data.data(), send_data.size()));
    };

    sync_wait(server() && client());
    EXPECT_EQ(send_data, std::string_view(recv_data));
}
