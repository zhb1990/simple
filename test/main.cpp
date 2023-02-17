#include <gtest/gtest.h>
#include <simple/coro/network.h>
#include <simple/coro/scheduler.h>
#include <simple/coro/thread_pool.h>
#include <simple/net/socket_system.h>
#include <simple/shm/shm_channel_select.h>

class test_env : public testing::Environment {
  public:
    void SetUp() override {
        using namespace simple;
        network::instance().init();
        scheduler::instance().start();
        shm_channel_select::instance().start();
        thread_pool::instance().start(1);
        socket_system::instance().start();
        socket_system::instance().register_signal_callback([this](int sig) { TearDown(); });
    }

    void TearDown() override {
        using namespace simple;
        shm_channel_select::instance().stop();
        socket_system::instance().stop();
        scheduler::instance().stop();
        thread_pool::instance().stop();
    }
};

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);

    testing::AddGlobalTestEnvironment(new test_env);

    return RUN_ALL_TESTS();
}
