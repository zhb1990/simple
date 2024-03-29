﻿#include "gate_master.h"

#include <google/protobuf/util/json_util.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <proto_utils.h>
#include <simple/coro/network.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <algorithm>
#include <ctime>
#include <headers.hpp>
#include <random>
#include <simple/coro/co_start.hpp>

gate_master::gate_master(const simple::toml_value_t* value) {
    if (!value->is_table()) {
        throw std::logic_error("gate master need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("listen_port"); it != args.end() && it->second.is_integer()) {
        listen_port_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("gate master need listen port");
    }
}

simple::task<> gate_master::awake() {
    simple::warn("[{}] awake", name());
    auto& network = simple::network::instance();
    auto server = co_await network.tcp_listen("", listen_port_, true);
    simple::co_start([this, server] { return accept(server); });
}

simple::task<> gate_master::accept(uint32_t server) {
    auto& network = simple::network::instance();
    for (;;) {
        const auto socket = co_await network.accept(server);
        simple::warn("[{}] accept socket {}", name(), socket);
        socket_data data{socket, nullptr, time(nullptr)};
        sockets_.emplace(socket, data);
        simple::co_start([socket, this] { return socket_start(socket); });
        simple::co_start([socket, this] { return socket_check(socket); });
    }
}

simple::task<> gate_master::socket_start(uint32_t socket) {
    const auto it = sockets_.find(socket);
    if (it == sockets_.end()) {
        co_return;
    }

    try {
        simple::memory_buffer buffer;
        net_header header{};
        for (;;) {
            co_await recv_net_buffer(buffer, header, socket);
            const uint32_t len = header.len;
            simple::info("[{}] socket:{} recv id:{} session:{} len:{}", name(), socket, header.id, header.session, len);
            it->second.last_recv = time(nullptr);
            forward_message(it->second, header.id, header.session, buffer);
        }
    } catch (std::exception& e) {
        simple::error("[{}] socket:{} exception {}", name(), socket, ERROR_CODE_MESSAGE(e.what()));
    }

    // 断网处理
    if (it->second.data && it->second.data->socket == socket) {
        gate_disconnect(it->second.data);
    }
    sockets_.erase(it);
}

simple::task<> gate_master::socket_check(uint32_t socket) {
    constexpr int64_t auto_close_session = 180;
    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution dis(auto_close_session / 3, auto_close_session * 4 / 3);
    auto& network = simple::network::instance();
    for (;;) {
        co_await simple::sleep_for(std::chrono::seconds(dis(engine)));
        auto it = sockets_.find(socket);
        if (it == sockets_.end()) {
            break;
        }

        if (time(nullptr) - it->second.last_recv > auto_close_session) {
            simple::warn("[{}] close socket:{} by check", name(), socket);
            network.close(socket);
            break;
        }
    }
}

void gate_master::gate_disconnect(gate_data* gate) const {
    gate->socket = 0;

    for (auto* s : gate->services) {
        s->online = false;
    }

    publish(gate);
}

static void add_gate_info(google::protobuf::RepeatedPtrField<game::s_gate_info>* gates, const gate_data* gate) {
    auto* info = gates->Add();
    *info->mutable_addresses() = gate->addresses;
    info->set_id(gate->id);
    auto* services = info->mutable_services();
    for (const auto* s : gate->services) {
        auto* service = services->Add();
        service->set_id(s->id);
        service->set_tp(static_cast<game::service_type>(s->tp));
        service->set_online(s->online);
    }
}

void gate_master::publish(gate_data* gate) const {
    auto& network = simple::network::instance();
    game::s_gate_register_brd brd;
    auto* gates = brd.mutable_gates();
    add_gate_info(gates, gate);
    auto buf = create_net_buffer(game::id_s_gate_register_brd, 0, brd);

    for (auto& [id, g] : gates_) {
        if (g.socket == 0 || &g == gate) {
            continue;
        }

        network.write(g.socket, buf);
    }
}

void gate_master::send(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg) {
    const auto buf = create_net_buffer(id, session, msg);
    auto& network = simple::network::instance();
    network.write(socket, buf);
}

void gate_master::forward_message(socket_data& socket, uint16_t id, uint64_t session, const simple::memory_buffer& buffer) {
    // 总共只会处理3个消息，直接switch
    switch (id) {
        case game::id_s_gate_register_req:
            return gate_register(socket, session, buffer);
        case game::id_s_service_update_req:
            return gate_upload(socket, session, buffer);
        case game::id_s_ping_req:
            return proc_ping(socket.socket, session, buffer);
        default:
            break;
    }
}

void gate_master::gate_register(socket_data& socket, uint64_t session, const simple::memory_buffer& buffer) {
    game::s_gate_register_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        return;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(req, &log_str);
    simple::info("[{}] register req:{}", name(), log_str);

    game::s_gate_register_ack ack;
    auto& result = *ack.mutable_result();
    const auto info = req.info();
    const auto id = static_cast<uint16_t>(info.id());
    const auto& services = info.services();
    // 先检查service是否能被加入到gate
    if (!check_services(services, id)) {
        result.set_ec(game::ec_system);
        return send(socket.socket, game::id_s_gate_register_ack, session, ack);
    }

    gate_data* gate;
    if (socket.data) {
        if (socket.data->id != id) {
            simple::error("[{}]  socket:{} registered gate:{} != {}", name(), socket.socket, socket.data->id, id);
            result.set_ec(game::ec_system);
            return send(socket.socket, game::id_s_gate_register_ack, session, ack);
        }
        gate = socket.data;
        if (gate->socket != socket.socket) {
            // 断开之前的连接
            if (const auto it = sockets_.find(gate->socket); it != sockets_.end()) {
                it->second.data = nullptr;
                auto& network = simple::network::instance();
                network.close(gate->socket);
            }
        }
    } else {
        // 新增一个gate
        gate_data temp{.id = id};
        const auto [fst, snd] = gates_.emplace(id, temp);
        gate = &fst->second;
        socket.data = gate;
    }

    gate->socket = socket.socket;
    gate->addresses = info.addresses();
    add_services(services, gate);

    simple::warn("[{}] socket:{} gate:{} register succ", name(), socket.socket, id);
    result.set_ec(game::ec_success);
    auto* gates = ack.mutable_gates();
    for (auto& [i, g] : gates_) {
        if (&g != gate) {
            add_gate_info(gates, &g);
        }
    }
    send(socket.socket, game::id_s_gate_register_ack, session, ack);
    publish(gate);
}

void gate_master::gate_upload(socket_data& socket, uint64_t session, const simple::memory_buffer& buffer) {
    game::s_service_update_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        return;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(req, &log_str);
    simple::info("[{}] upload req:{}", name(), log_str);

    game::msg_common_ack ack;
    auto& result = *ack.mutable_result();
    if (!socket.data) {
        // 该连接还未绑定gate
        result.set_ec(game::ec_system);
        return send(socket.socket, game::id_s_service_update_ack, session, ack);
    }

    const auto& services = req.services();
    // 先检查service是否能被加入到gate
    if (!check_services(services, socket.data->id)) {
        result.set_ec(game::ec_system);
        return send(socket.socket, game::id_s_service_update_ack, session, ack);
    }

    add_services(services, socket.data);
    result.set_ec(game::ec_success);
    send(socket.socket, game::id_s_service_update_ack, session, ack);
    publish(socket.data);
}

bool gate_master::check_services(const google::protobuf::RepeatedPtrField<game::s_service_info>& services, uint16_t gate) {
    return std::ranges::all_of(services, [this, gate](const auto& service) {
        if (const auto it = services_.find(service.id()); it != services_.end() && it->second.gate != gate) {
            return false;
        }

        return true;
    });
}

void gate_master::add_services(const google::protobuf::RepeatedPtrField<game::s_service_info>& services, gate_data* gate) {
    for (const auto& s : services) {
        const auto it = services_.find(s.id());
        if (it != services_.end()) {
            it->second.online = s.online();
            continue;
        }

        service_data temp{static_cast<uint16_t>(s.id()), static_cast<uint16_t>(s.tp()), gate->id, s.online()};
        const auto result = services_.emplace(temp.id, temp);
        gate->services.emplace_back(&result.first->second);
    }
}

SIMPLE_SERVICE_API simple::service* gate_master_create(const simple::toml_value_t* value) { return new gate_master(value); }

SIMPLE_SERVICE_API void gate_master_release(const simple::service* t) { delete t; }
