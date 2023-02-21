#include "proxy.h"

#include <gate_connector.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>
#include <simple/utils/time.h>

#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>
#include <stdexcept>

proxy::proxy(const simple::toml_value_t* value) {
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
            *this, &it->second, game::st_proxy, [this] { return on_register_to_gate(); },
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

simple::task<> proxy::on_register_to_gate() { return gate_connector_->subscribe(game::st_login); }

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
        return;
    }

    // todo: 处理gate通过共享内存转发的协议
}

void proxy::forward_player(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer) {
    // 处理玩家的协议
    game::s_client_forward_brd brd;
    brd.set_gate(this->id());
    brd.set_socket(socket.socket);
    brd.set_data(buffer.begin_read(), buffer.readable());

    uint16_t dest_service = 0;
    // 直接先if判断下
    // 注册登录发给login
    if (id == game::id_login_req) {
        // 判断下是否已经收到登录协议，已经收到，必须建立新连接才能再次发送登录请求
        // 可以直接将网络断开，让客户端重试
        if (socket.userid > 0 || socket.wait_login) {
            const auto utf8 = ERROR_CODE_MESSAGE("同一个连接重复发登录请求");
            simple::network::instance().close(socket.socket);
            return;
        }

        // 随机一个login去处理
        dest_service = gate_connector_->rand_subscribe(game::st_login);
        socket.wait_login = true;
    } else if (id == game::id_ping_req) {
        if (socket.wait_login) {
            // todo: 如果还没登录，ping包直接回复
            return;
        }

        // 已经登录了发给逻辑服
        dest_service = socket.logic;
    } else if (id == game::id_enter_room_req || id == game::id_move_req) {
        // 直接发给room房间
        // 判断下是否有房间id
        if (socket.room <= 0) {
            // todo: 按消息id，回复不同的ack
            // 只要所有的ack第一项都是 ack_result result = 1;
            // 可以统一回复msg_common_ack
            return;
        }

        dest_service = socket.room;
    } else {
        // 其他的发给逻辑服
        if (socket.wait_login) {
            // todo: 还没登录完可以缓存下来等登录成功
            return;
        }

        dest_service = socket.logic;
    }

    gate_connector_->write(dest_service, 0, game::id_s_client_forward_brd, brd);
}

SIMPLE_SERVICE_API simple::service_base* proxy_create(const simple::toml_value_t* value) { return new proxy(value); }

SIMPLE_SERVICE_API void proxy_release(const simple::service_base* t) { delete t; }
