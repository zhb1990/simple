#include "simple/application/frame_awaiter.h"
#include "simple/application/service.hpp"
#include "simple/coro/co_start.hpp"
#include "simple/coro/network.h"
#include "simple/log/log.h"
#include "simple/shm/shm_channel.h"
#include "simple/utils/toml_types.hpp"
#include "simple/web/websocket.h"

class test2 final : public simple::service {
  public:
    simple::task<> awake() override;
    simple::task<> update() override;

  private:
    uint32_t client_{0};
};

SIMPLE_SERVICE_API simple::service* test2_create(const simple::toml_value_t&) { return new test2(); }

SIMPLE_SERVICE_API void test2_release(const simple::service* t) { delete t; }

simple::task<> test2::awake() {
    simple::error("test2::awake()");
    client_ = co_await simple::network::instance().kcp_connect("127.0.0.1", "10034", std::chrono::seconds{4});
    simple::websocket ws(simple::websocket_type::client, client_);
    co_await ws.handshake("127.0.0.1");

    simple::co_start([]() -> simple::task<> {
        simple::shm_channel channel("test2", "test1", 16);
        constexpr std::string_view data = "hello world";
        for (;;) {
            co_await channel.write(data.data(), static_cast<uint32_t>(data.size()));
            co_await simple::skip_frame(10);
            simple::error("channel write {}", data);
        }
    });
}

simple::task<> test2::update() {
    simple::error("test2::update()");
    constexpr std::string_view data = "hello test2";
    simple::websocket ws(simple::websocket_type::client, client_);
    ws.write(simple::websocket_opcode::text, data.data(), data.size(), 2);
    co_return;
}
