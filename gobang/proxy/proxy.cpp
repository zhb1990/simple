#include "proxy.h"

#include <gate_connector.h>
#include <msg_client.pb.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
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
        throw std::logic_error("proxy need gate config");
    }

    fn_client_msgs_.emplace(game::id_login_req,
                            [this](const socket_data& socket, uint64_t session, const simple::memory_buffer& buffer) {
                                return client_register_msg(socket, session, buffer);
                            });
    fn_client_msgs_.emplace(game::id_enter_room_req,
                            [this](const socket_data& socket, uint64_t session, const simple::memory_buffer& buffer) {
                                return client_room_msg(socket, game::id_enter_room_req, session, buffer);
                            });
    fn_client_msgs_.emplace(game::id_move_req,
                            [this](const socket_data& socket, uint64_t session, const simple::memory_buffer& buffer) {
                                return client_room_msg(socket, game::id_move_req, session, buffer);
                            });

    fn_on_client_forward_brd_.emplace(
        game::id_login_ack,
        [this](const socket_data& socket, const game::s_client_forward_brd& brd) { return client_login_ack(socket, brd); });
    fn_on_client_forward_brd_.emplace(game::id_match_ack, proxy::client_match_ack);
    fn_on_client_forward_brd_.emplace(game::id_move_ack, proxy::client_move_ack);
    fn_on_client_forward_brd_.emplace(game::id_move_brd, proxy::client_move_brd);
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

            forward_client(*it, header.id, header.session, buffer);
        }
    } catch (std::exception& e) {
        simple::error("[{}] socket:{} exception {}", name(), socket, ERROR_CODE_MESSAGE(e.what()));
    }

    // 断网处理, 通知逻辑服务器
    if (it->logic > 0) {
        report_client_offline(*it);
    }

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
    // 处理gate通过共享内存转发的协议
    // 只有转发给客户端的消息 和 踢掉玩家连接的消息，直接if判断
    if (id == game::id_s_kick_client_req) {
        return kick_client(from, session, buffer);
    }

    if (id == game::id_s_client_forward_brd) {
        return client_forward_brd(buffer);
    }
}

void proxy::forward_client(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer) {
    // 处理玩家的协议
    if (const auto it = fn_client_msgs_.find(id); it != fn_client_msgs_.end()) {
        return it->second(socket, session, buffer);
    }

    return client_other_msg(socket, id, session, buffer);
}

void proxy::client_register_msg(const socket_data& socket, uint64_t session, const simple::memory_buffer& buffer) {
    // 判断下是否已经收到登录协议，已经收到，必须建立新连接才能再次发送登录请求
    // 可以直接将网络断开，让客户端重试
    if (socket.userid > 0 || socket.wait_login) {
        const auto utf8 = ERROR_CODE_MESSAGE("同一个连接重复发登录请求");
        simple::error("[{}] socket:{} {}", name(), socket.socket, utf8);
        return simple::network::instance().close(socket.socket);
    }

    socket.wait_login = true;
    // 随机一个login去处理
    const auto dest_service = gate_connector_->rand_subscribe(game::st_login);
    send_to_service(dest_service, socket.socket, game::id_login_req, session, buffer);
}

void proxy::client_room_msg(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer) {
    // 直接发给room房间
    // 判断下是否有房间id
    if (socket.room <= 0) {
        // 按消息id，回复不同的ack
        // 只要所有的ack第一项都是 ack_result result = 1;
        // 可以统一回复msg_common_ack
        game::msg_common_ack ack;
        ack.mutable_result()->set_ec(game::ec_no_match);
        return send_to_client(socket.socket, static_cast<uint16_t>((id & 0xfff) | game::message_type::msg_s2c_ack), session,
                              ack);
    }

    send_to_service(socket.room, socket.socket, id, session, buffer);
}

static void init_client_forward_brd(game::s_client_forward_brd& brd, uint16_t service, uint32_t socket, uint16_t id,
                                    uint64_t session, const simple::memory_buffer& buffer) {
    brd.set_gate(service);
    brd.set_socket(socket);
    brd.set_id(id);
    brd.set_session(session);
    brd.set_data(buffer.begin_read(), buffer.readable());
}

void proxy::client_other_msg(const socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer) {
    // 其他的发给逻辑服
    if (socket.wait_login) {
        if (id == game::id_ping_req) {
            // ping包直接回复
            game::ping_req req;
            if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
                return;
            }

            game::ping_ack ack;
            ack.set_t1(req.t1());
            ack.set_t2(simple::get_system_clock_millis());
            return send_to_client(socket.socket, game::id_ping_ack, session, ack);
        }

        // 还没登录完可以缓存下来等登录成功
        game::s_client_forward_brd brd;
        init_client_forward_brd(brd, this->id(), socket.socket, id, session, buffer);
        socket.cache.emplace_back(std::move(brd));
        return;
    }

    return send_to_service(socket.logic, socket.socket, id, session, buffer);
}

void proxy::send_to_service(uint16_t dest_service, uint32_t socket, uint16_t id, uint64_t session,
                            const simple::memory_buffer& buffer) {
    game::s_client_forward_brd brd;
    init_client_forward_brd(brd, this->id(), socket, id, session, buffer);
    return gate_connector_->write(dest_service, 0, game::id_s_client_forward_brd, brd);
}

void proxy::send_to_client(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg) {
    temp_buffer_.clear();
    init_client_buffer(temp_buffer_, id, session, msg);
    return simple::websocket(simple::websocket_type::server, socket)
        .write(simple::websocket_opcode::binary, temp_buffer_.begin_read(), temp_buffer_.readable());
}

void proxy::client_forward_brd(const simple::memory_buffer& buffer) {
    game::s_client_forward_brd brd;
    if (!brd.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        return;
    }

    const auto socket = brd.socket();
    // 找到对应的
    const auto it = sockets_.find(socket);
    if (it == sockets_.end()) {
        // 可能是断网了
        return;
    }

    if (it->userid != brd.userid()) {
        // 可能gate重启了
        return;
    }

    const auto msg_id = brd.id();
    // 对特定的协议进行解析
    if (const auto it_fn = fn_on_client_forward_brd_.find(msg_id); it_fn != fn_on_client_forward_brd_.end()) {
        it_fn->second(*it, brd);
    }

    ws_header header{flag_valid, {}, static_cast<uint16_t>(msg_id), brd.session()};
    temp_buffer_.clear();
    temp_buffer_.append(&header, sizeof(header));
    const auto& data = brd.data();
    temp_buffer_.append(data.data(), data.size());
    return simple::websocket(simple::websocket_type::server, socket)
        .write(simple::websocket_opcode::binary, temp_buffer_.begin_read(), temp_buffer_.readable());
}

void proxy::report_client_offline(const socket_data& socket) {
    game::s_client_offline_brd brd;
    brd.set_socket(socket.socket);
    brd.set_gate(id());
    brd.set_userid(socket.userid);
    gate_connector_->write(socket.logic, 0, game::id_s_client_offline_brd, brd);
}

void proxy::client_login_ack(const socket_data& socket, const game::s_client_forward_brd& brd) {
    const auto& data = brd.data();
    game::login_ack ack;
    if (!ack.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        // 协议解析出错了，最好还是关闭连接，报个错
        const auto utf8 = ERROR_CODE_MESSAGE("登录回应解析出错");
        simple::error("[{}] socket:{} {}", name(), socket.socket, utf8);
        return simple::network::instance().close(socket.socket);
    }

    if (ack.result().ec() != game::ec_success) {
        // 登录失败了
        return;
    }

    // 登录成功了，记录下玩家的状态
    socket.logic = brd.logic();
    socket.wait_login = false;
    socket.room = ack.room();
    socket.userid = ack.userid();

    // 之前缓存的消息发给逻辑服
    for (const auto& msg : socket.cache) {
        gate_connector_->write(socket.logic, 0, game::id_s_client_forward_brd, msg);
    }
    socket.cache.clear();
}

void proxy::kick_client(uint16_t from, uint64_t session, const simple::memory_buffer& buffer) {
    game::msg_common_ack ack;
    game::s_kick_client_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        // 协议解析出错了, 给个回复
        ack.mutable_result()->set_ec(game::ec_system);
        gate_connector_->write(from, session, game::id_s_kick_client_ack, ack);
        return;
    }

    const auto socket = req.socket();
    if (const auto it = sockets_.find(socket); it != sockets_.end() && it->userid == req.userid()) {
        simple::network::instance().close(socket);
    }

    ack.mutable_result()->set_ec(game::ec_success);
    gate_connector_->write(from, session, game::id_s_kick_client_ack, ack);
}

void proxy::client_match_ack(const socket_data& socket, const game::s_client_forward_brd& brd) {
    const auto& data = brd.data();
    game::match_ack ack;
    if (!ack.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        // 协议解析出错了
        return;
    }

    if (ack.result().ec() != game::ec_success) {
        // 登录失败了
        return;
    }

    // 匹配成功了，记录下玩家的状态
    socket.room = ack.room();
}

void proxy::client_move_brd(const socket_data& socket, const game::s_client_forward_brd& brd) {
    const auto& data = brd.data();
    game::move_brd client_brd;
    if (!client_brd.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        // 协议解析出错了
        return;
    }

    if (client_brd.over() != game::over_type::none) {
        socket.room = 0;
    }
}

void proxy::client_move_ack(const socket_data& socket, const game::s_client_forward_brd& brd) {
    const auto& data = brd.data();
    game::move_ack ack;
    if (!ack.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        // 协议解析出错了
        return;
    }

    if (ack.result().ec() != game::ec_success) {
        // 登录失败了
        return;
    }

    if (ack.over() != game::over_type::none) {
        socket.room = 0;
    }
}

SIMPLE_SERVICE_API simple::service_base* proxy_create(const simple::toml_value_t* value) { return new proxy(value); }

SIMPLE_SERVICE_API void proxy_release(const simple::service_base* t) { delete t; }
