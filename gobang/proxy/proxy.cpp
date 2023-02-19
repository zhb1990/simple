#include "proxy.h"

#include <gate_connector.h>
#include <google/protobuf/util/json_util.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>
#include <simple/utils/time.h>

#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>
#include <stdexcept>

proxy::proxy(const simple::toml_value_t* value) : engine_(std::random_device{}()) {
    if (!value->is_table()) {
        throw std::logic_error("proxy need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("listen_port"); it != args.end() && it->second.is_integer()) {
        listen_port_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("proxy need listen port");
    }

    if (const auto it = args.find("gate"); it != args.end()) {
        gate_connector_ = std::make_shared<gate_connector>(
            *this, &it->second, game::st_proxy,
            [this](uint32_t socket, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
                return forward_gate(socket, session, id, buffer);
            },
            [this](uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
                return forward_shm(from, session, id, buffer);
            });
    } else {
        throw std::logic_error("proxy need listen port");
    }

    // todo: 删除
    // 测试下gate转发
    if (const auto it = args.find("test_proxy"); it != args.end() && it->second.is_integer()) {
        const auto test_id = static_cast<uint16_t>(it->second.as_integer());
        simple::co_start([this, test_id]() -> simple::task<> {
            for (;;) {
                game::s_ping_req req;
                const auto last = simple::get_system_clock_millis();
                req.set_t1(last);
                [[maybe_unused]] const auto ack =
                    co_await gate_connector_->call<game::s_ping_ack>(test_id, game::id_s_ping_req, req);
                simple::warn("[{}] test proxy ping {}->{} delay {}ms", name(), id(), test_id,
                             simple::get_system_clock_millis() - last);
                co_await simple::sleep_for(std::chrono::seconds{5});
            }
        });
    }
}

simple::task<> proxy::awake() {
    gate_connector_->start();

    // 订阅login服务
    simple::co_start([this] { return subscribe_login(); });

    auto& network = simple::network::instance();
    auto server = co_await network.tcp_listen("", listen_port_, true);
    simple::co_start([this, server] { return accept(server); });
}

simple::task<> proxy::accept(uint32_t server) {
    auto& network = simple::network::instance();
    for (;;) {
        const auto socket = co_await network.accept(server);
        simple::warn("[{}] accept socket:{}", name(), socket);
        socket_data data;
        data.socket = socket;
        data.last_recv = time(nullptr);
        sockets_.emplace(data);
        simple::co_start([socket, this] { return socket_start(socket); });
        simple::co_start([socket, this] { return socket_check(socket); });
    }
}

simple::task<> proxy::socket_start(uint32_t socket) {
    const auto it = sockets_.find(socket);
    if (it == sockets_.end()) {
        co_return;
    }

    try {
        const simple::websocket ws(simple::websocket_type::server, socket);
        // 先握手
        co_await ws.handshake();
        simple::memory_buffer buffer;
        for (;;) {
            const auto op = co_await ws.read(buffer);
            it->last_recv = time(nullptr);

            if (op == simple::websocket_opcode::close) {
                simple::network::instance().close(socket);
                throw std::logic_error("websocket close");
            }

            if (op == simple::websocket_opcode::ping) {
                ws.write(simple::websocket_opcode::pong, buffer.begin_read(), buffer.readable());
                continue;
            }

            if (is_websocket_control(op)) {
                // 其他控制帧，不做处理
                continue;
            }

            const ws_header& header = *reinterpret_cast<ws_header*>(buffer.begin_read());
            buffer.read(sizeof(header));
            simple::info("[{}] socket:{} recv id:{} session:{}", name(), socket, header.id, header.session);

            forward_player(*it, header.id, header.session, buffer);
        }
    } catch (std::exception& e) {
        simple::error("[{}] socket:{} exception {}", name(), socket, ERROR_CODE_MESSAGE(e.what()));
    }

    // todo: 断网处理, 通知逻辑服务器

    sockets_.erase(it);
}

simple::task<> proxy::socket_check(uint32_t socket) {
    constexpr int64_t auto_close_session = 180;
    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution dis(auto_close_session / 3, auto_close_session * 4 / 3);
    for (;;) {
        co_await simple::sleep_for(std::chrono::seconds(dis(engine)));
        auto it = sockets_.find(socket);
        if (it == sockets_.end()) {
            break;
        }

        if (time(nullptr) - it->last_recv > auto_close_session) {
            simple::warn("[{}] close socket:{} by check", name(), socket);
            simple::network::instance().close(socket);
            break;
        }
    }
}

simple::task<> proxy::subscribe_login() {
    using namespace std::chrono_literals;
    auto timeout = []() -> simple::task<> { co_await simple::sleep_for(5s); };
    game::s_service_subscribe_req req;
    req.set_tp(game::st_login);

    for (;;) {
        try {
            if (!gate_connector_->is_socket_valid()) {
                co_await simple::sleep_for(2s);
                continue;
            }

            auto call_result = co_await (
                gate_connector_->call_gate<game::s_service_subscribe_ack>(game::id_s_service_subscribe_req, req) || timeout());
            if (call_result.index() == 1) {
                simple::error("[{}] subscribe login timeout.", name());
                continue;
            }

            const auto& ack = std::get<0>(call_result);
            auto& result = ack.result();
            if (auto ec = result.ec(); ec != game::ec_success) {
                simple::error("[{}] subscribe login ec:{} {}", name(), ec, result.msg());
                continue;
            }

            for (auto& s : ack.services()) {
                update_login(s);
            }

            co_return;
        } catch (std::exception& e) {
            simple::error("[{}] subscribe login exception {}", name(), ERROR_CODE_MESSAGE(e.what()));
        }
    }
}

void proxy::update_login(const game::s_service_info& service) {
    if (service.tp() != game::st_login) {
        return;
    }

    const auto id = static_cast<uint16_t>(service.id());
    for (auto& s : logins_) {
        if (s.id == id) {
            s.online = service.online();
            return;
        }
    }

    logins_.emplace_back(id, service.online());
}

uint16_t proxy::rand_login() {
    std::vector<uint16_t> online;
    online.reserve(logins_.size());
    for (const auto& s : logins_) {
        if (s.online) {
            online.emplace_back(s.id);
        }
    }

    const auto size = online.size();
    if (size == 0) {
        return 0;
    }

    if (size == 1) {
        return online[0];
    }

    std::uniform_int_distribution<uint16_t> dis(0, static_cast<uint16_t>(size - 1));
    return online[dis(engine_)];
}

void proxy::forward_gate([[maybe_unused]] uint32_t socket, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
    // 只有订阅的广播
    if (id != game::id_s_service_subscribe_brd) {
        return;
    }

    game::s_service_subscribe_brd brd;
    if (!brd.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("[{}] parse s_service_subscribe_brd fail", name());
        return;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(brd, &log_str);
    simple::info("[{}] subscribe brd:{}", name(), log_str);

    for (auto& s : brd.services()) {
        update_login(s);
    }
}

void proxy::forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
    // todo: 删除
    // 测试下gate转发
    if (id == game::id_s_ping_req) {
        game::s_ping_req req;
        if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
            return;
        }

        game::s_ping_ack ack;
        ack.set_t1(req.t1());
        ack.set_t2(simple::get_system_clock_millis());

        gate_connector_->write(from, session, game::id_s_ping_ack, ack);
    }

    // todo: 处理gate通过共享内存转发的协议
}

void proxy::forward_player(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer) {
    // todo: 处理玩家的协议
}

SIMPLE_SERVICE_API simple::service_base* proxy_create(const simple::toml_value_t* value) { return new proxy(value); }

SIMPLE_SERVICE_API void proxy_release(const simple::service_base* t) { delete t; }
