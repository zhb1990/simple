#include <simple/coro/network.h>
#include <simple/log/log.h>
#include <simple/shm/shm_channel.h>
#include <simple/utils/os.h>
#include <simple/web/websocket.h>

#include <simple/application/service.hpp>
#include <simple/coro/co_start.hpp>
#include <simple/utils/toml_types.hpp>

class test1 final : public simple::service_base {
  public:
    simple::task<> awake() override;
    simple::task<> update() override;

    simple::task<> accept(uint32_t server);

    simple::task<> session_start(uint32_t session);

  private:
    int32_t value_{0};
    int32_t accepted_{0};
};

simple::task<> test1::awake() {
    simple::error("test1::awake()");
    auto& network = simple::network::instance();
    auto server = co_await network.kcp_listen("127.0.0.1", 10034, true);
    simple::co_start([this, server] { return accept(server); });
    simple::co_start([]() -> simple::task<> {
        simple::shm_channel channel("test1", "test2", 16);
        for (;;) {
            auto ptr = co_await channel.read();
            simple::error("channel read {}", std::string_view(*ptr));
        }
    });
}

simple::task<> test1::update() {
    simple::warn("test1 value:{} accepted:{}", value_, accepted_);
    co_return;
}

simple::task<> test1::accept(uint32_t server) {
    auto& network = simple::network::instance();
    for (;;) {
        const auto session = co_await network.accept(server);
        simple::error("accept session {}", session);
        ++accepted_;
        simple::co_start([session, this] { return session_start(session); });
    }
}

simple::task<> test1::session_start(uint32_t session) {
    try {
        auto& network = simple::network::instance();
        simple::websocket ws(simple::websocket_type::server, session);
        co_await ws.handshake();
        simple::memory_buffer buf;
        for (;;) {
            const auto op = co_await ws.read(buf);
            simple::error("session {} recv op:{} data:{}", session, static_cast<int32_t>(op), std::string_view(buf));
            ++value_;
        }
    } catch (std::exception& e) {
        simple::error("session {} exception {}", session, ERROR_CODE_MESSAGE(e.what()));
    }

    --accepted_;
}

DS_SERVICE_API simple::service_base* test1_create(const simple::toml_value_t*) { return new test1(); }

DS_SERVICE_API void test1_release(const simple::service_base* t) { delete t; }
