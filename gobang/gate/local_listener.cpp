#include "local_listener.h"

#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <proto_utils.h>
#include <simple/coro/network.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>
#include <simple/utils/time.h>

#include <algorithm>
#include <ctime>
#include <headers.hpp>
#include <random>
#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>

#include "gate.h"

local_listener::local_listener(gate& g) : gate_(g) {}

simple::task<> local_listener::start() {
    auto& network = simple::network::instance();
    auto server = co_await network.tcp_listen("127.0.0.1", gate_.local_port(), true);
    simple::co_start([this, server] { return accept(server); });
}

void local_listener::publish(const service_data* data) {
    auto& info = gate_.get_service_type_info(data->tp);
    if (info.subscribe.empty()) {
        return;
    }

    auto& network = simple::network::instance();
    game::s_service_subscribe_brd brd;
    data->to_proto(*brd.add_services());
    auto buf = create_net_buffer(game::id_s_service_subscribe_brd, 0, brd);
    for (auto s : info.subscribe) {
        if (const auto it = service_sockets_.find(s); it != service_sockets_.end()) {
            network.write(it->second->socket, buf);
        }
    }
}

simple::task<> local_listener::accept(uint32_t server) {
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

simple::task<> local_listener::socket_start(const local_listener::socket_data_ptr& ptr) {
    try {
        simple::memory_buffer buffer;
        net_header header{};
        for (;;) {
            co_await recv_net_buffer(buffer, header, ptr->socket);
            const uint32_t len = header.len;
            simple::info("[{}] socket:{} recv id:{} session:{} len:{}", gate_.name(), ptr->socket, header.id, header.session,
                         len);
            ptr->last_recv = time(nullptr);
            forward_message(ptr, header.id, header.session, buffer);
        }
    } catch (std::exception& e) {
        simple::error("[{}] socket:{} exception {}", gate_.name(), ptr->socket, ERROR_CODE_MESSAGE(e.what()));
    }

    ptr->socket = 0;
    if (const auto it = service_sockets_.find(ptr->service); it != service_sockets_.end() && it->second == ptr) {
        service_sockets_.erase(it);
        // service掉线
        co_await gate_.local_service_online_changed(ptr->service, false);
    }
}

simple::task<> local_listener::socket_check(const local_listener::socket_data_ptr& ptr) {
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

void local_listener::forward_message(const local_listener::socket_data_ptr& ptr, uint16_t id, uint64_t session,
                                     const simple::memory_buffer& buffer) {
    switch (id) {
        case game::id_s_service_register_req:
            return service_register(ptr, session, buffer);
        case game::id_s_service_subscribe_req:
            return service_subscribe(ptr, session, buffer);
        case game::id_s_ping_req:
            return proc_ping(ptr->socket, session, buffer);

        default:
            break;
    }
}

void local_listener::service_register(const local_listener::socket_data_ptr& ptr, uint64_t session,
                                      const simple::memory_buffer& buffer) {
    auto req = std::make_shared<game::s_service_register_req>();
    if (!req->ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("[{}] socket:{} parse s_service_register_req fail", gate_.name(), ptr->socket);
        simple::network::instance().close(ptr->socket);
        return;
    }

    simple::co_start([this, req, ptr, session]() { return service_register(ptr, session, *req); });
}

simple::task<> local_listener::service_register(const local_listener::socket_data_ptr& ptr, uint64_t session,
                                                const game::s_service_register_req& req) {
    if (ptr->socket == 0) {
        // 这个连接掉线了，可以不用处理了
        co_return;
    }

    game::s_service_register_ack ack;
    ack.set_gate(gate_.id());
    auto& result = *ack.mutable_result();
    auto& info = req.info();
    const auto service = static_cast<uint16_t>(info.id());
    if (service == 0) {
        simple::error("[{}] socket:{} registered service:{} is invalid.", gate_.name(), ptr->socket, service);
        result.set_ec(game::ec_system);
        send(ptr->socket, game::id_s_service_register_ack, session, ack);
        co_return;
    }

    if (service == ptr->service) {
        // 说明之前已经注册成功了
        result.set_ec(game::ec_success);
        send(ptr->socket, game::id_s_service_register_ack, session, ack);
        co_return;
    }

    if (ptr->service != 0) {
        // 说明之前已经注册成功了其他的服务id
        result.set_ec(game::ec_system);
        send(ptr->socket, game::id_s_service_register_ack, session, ack);
        co_return;
    }

    // 向gate master上传，成功了才继续处理
    game::s_service_info temp = info;
    temp.set_online(false);
    auto timeout = []() -> simple::task<> { co_await simple::sleep_for(std::chrono::seconds(5)); };
    const auto rpc_result = co_await (gate_.upload_to_master(temp) || timeout());
    const auto* ec = std::get_if<0>(&rpc_result);
    if (!ec) {
        // 超时了
        result.set_ec(game::ec_timeout);
        send(ptr->socket, game::id_s_service_register_ack, session, ack);
        co_return;
    }

    if (*ec != game::ec_success) {
        result.set_ec(*ec);
        send(ptr->socket, game::id_s_service_register_ack, session, ack);
        co_return;
    }

    // 注册服务
    auto* service_ptr = gate_.add_local_service(req);

    if (const auto it = service_sockets_.find(service); it != service_sockets_.end() && it->second != ptr) {
        auto last = it->second;
        last->service = 0;
        service_sockets_.erase(it);
        simple::network::instance().close(last->socket);
    }

    simple::error("[{}] socket:{} registered service:{} succ.", gate_.name(), ptr->socket, service);

    if (ptr->socket == 0) {
        // 发布服务状态给订阅的
        publish(service_ptr);
        co_return;
    }

    ptr->service = service;
    service_sockets_[service] = ptr;
    result.set_ec(game::ec_success);
    send(ptr->socket, game::id_s_service_register_ack, session, ack);

    // 改为在线状态
    co_await gate_.local_service_online_changed(service, true);
}

void local_listener::service_subscribe(const local_listener::socket_data_ptr& ptr, uint64_t session,
                                       const simple::memory_buffer& buffer) {
    game::s_service_subscribe_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("local_listener socket {} parse s_service_subscribe_req fail", ptr->socket);
        return simple::network::instance().close(ptr->socket);
    }

    game::s_service_subscribe_ack ack;
    auto& result = *ack.mutable_result();
    // 验证连接是否已经注册
    if (ptr->service == 0) {
        result.set_ec(game::ec_system);
        return send(ptr->socket, game::id_s_service_subscribe_ack, session, ack);
    }

    // 订阅特定类型的所有服务在线状态
    auto& info = gate_.get_service_type_info(req.tp());
    if (const auto it = std::ranges::find(info.subscribe, ptr->service); it == info.subscribe.end()) {
        info.subscribe.emplace_back(ptr->service);
    }

    for (auto* s : info.services) {
        s->to_proto(*ack.add_services());
    }

    result.set_ec(game::ec_success);
    return send(ptr->socket, game::id_s_service_subscribe_ack, session, ack);
}

void local_listener::send(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg) {
    auto buf = create_net_buffer(id, session, msg);
    auto& network = simple::network::instance();
    network.write(socket, buf);
}
