#include "remote_listener.h"

#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <proto_utils.h>
#include <simple/coro/network.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <ctime>
#include <headers.hpp>
#include <random>
#include <simple/coro/co_start.hpp>

#include "gate.h"

remote_listener::remote_listener(gate& g) : gate_(g) {}

simple::task<> remote_listener::start() {
    auto& network = simple::network::instance();
    auto server = co_await network.tcp_listen("", gate_.remote_port(), true);
    simple::co_start([this, server] { return accept(server); });
}

simple::task<> remote_listener::accept(uint32_t server) {
    auto& network = simple::network::instance();
    for (;;) {
        const auto socket = co_await network.accept(server);
        simple::warn("[{}] accept socket:{}", gate_.name(), socket);
        auto ptr = std::make_shared<socket_data>();
        ptr->socket = socket;
        ptr->last_recv = time(nullptr);
        simple::co_start([ptr, this] { return socket_start(ptr); });
        simple::co_start([ptr, this] { return socket_check(ptr); });
    }
}

simple::task<> remote_listener::socket_start(const socket_data_ptr& ptr) {
    try {
        simple::memory_buffer buffer;
        net_header header{};
        for (;;) {
            co_await recv_net_buffer(buffer, header, ptr->socket);
            const uint32_t len = header.len;
            simple::info("[{}] socket:{} recv id:{} session:{} len:{}", gate_.name(), ptr->socket, header.id, header.session,
                         len);
            ptr->last_recv = time(nullptr);
            forward_message(ptr->socket, header.id, header.session, buffer);
        }
    } catch (std::exception& e) {
        simple::error("[{}] socket:{} exception {}", gate_.name(), ptr->socket, ERROR_CODE_MESSAGE(e.what()));
    }

    ptr->socket = 0;
}

simple::task<> remote_listener::socket_check(const socket_data_ptr& ptr) const {
    constexpr int64_t auto_close_session = 180;
    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution dis(auto_close_session / 3, auto_close_session * 4 / 3);
    const auto socket = ptr->socket;
    auto& network = simple::network::instance();

    co_await simple::sleep_for(std::chrono::seconds(dis(engine)));
    while (ptr->socket > 0) {
        if (time(nullptr) - ptr->last_recv > auto_close_session) {
            simple::warn("[{}] close socket:{} by check", gate_.name(), socket);
            network.close(socket);
            break;
        }

        co_await simple::sleep_for(std::chrono::seconds(dis(engine)));
    }
}

void remote_listener::forward_message(uint32_t socket, uint16_t id, uint64_t session,
                                      const simple::memory_buffer& buffer) const {
    // 目前只需要处理其他gate发来的转发消息 和 ping包
    switch (id) {
        case game::id_s_gate_forward_brd:
            return gate_forward(socket, buffer);
        case game::id_s_ping_req:
            return proc_ping(socket, session, buffer);
        default:
            break;
    }
}

void remote_listener::gate_forward(uint32_t socket, const simple::memory_buffer& buffer) const {
    game::s_gate_forward_brd brd;
    if (!brd.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("[{}] socket:{} parse s_gate_forward_brd fail", gate_.name(), socket);
        simple::network::instance().close(socket);
        return;
    }

    gate_.forward(brd);
}
