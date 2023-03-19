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
#include <ranges>
#include <simple/application/service.hpp>
#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>

local_listener::local_listener(simple::service& s, const simple::toml_table_t& table) : service_(s) {
    if (const auto it = table.find("local_port"); it != table.end() && it->second.is_integer()) {
        local_port_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("gate need local listen port");
    }

    update_service_ = service_.events().register_handler<service_update_event>(&local_listener::update_service, this);
    service_.router().register_call("local_services", &local_listener::local_services, this);
}

simple::task<> local_listener::start() {
    auto& network = simple::network::instance();
    auto server = co_await network.tcp_listen("127.0.0.1", local_port_, true);
    simple::co_start([this, server] { return accept(server); });
}

void local_listener::update_service(const service_update_event& data) {
    const auto it = subscribe_infos_.find(data.info->tp);
    if (it == subscribe_infos_.end()) {
        return;
    }

    auto& network = simple::network::instance();
    game::s_service_subscribe_brd brd;
    data.info->to_proto(*brd.add_services());
    const auto buf = create_net_buffer(game::id_s_service_subscribe_brd, 0, brd);
    for (const auto& s : it->second) {
        network.write(s->socket, buf);
    }
}

simple::task<> local_listener::accept(uint32_t server) {
    auto& network = simple::network::instance();
    for (;;) {
        const auto socket = co_await network.accept(server);
        simple::warn("[{}] accept socket:{}", service_.name(), socket);
        auto ptr = std::make_shared<socket_data>();
        ptr->socket = socket;
        ptr->last_recv = time(nullptr);
        simple::co_start([ptr, this] { return socket_start(ptr); });
        simple::co_start([ptr, this] { return socket_check(ptr); });
    }
}

simple::task<> local_listener::socket_start(const socket_ptr& ptr) {
    try {
        simple::memory_buffer buffer;
        net_header header{};
        for (;;) {
            co_await recv_net_buffer(buffer, header, ptr->socket);
            const uint32_t len = header.len;
            simple::info("[{}] socket:{} recv id:{} session:{} len:{}", service_.name(), ptr->socket, header.id, header.session,
                         len);
            ptr->last_recv = time(nullptr);
            forward_message(ptr, header.id, header.session, buffer);
        }
    } catch (std::exception& e) {
        simple::error("[{}] socket:{} exception {}", service_.name(), ptr->socket, ERROR_CODE_MESSAGE(e.what()));
    }

    auto* service = ptr->service;
    if (service && service->socket == ptr->socket) {
        service->socket = 0;
        service->online = false;
        service->update();
    }

    for (const auto tp : ptr->subscribe) {
        const auto it = subscribe_infos_.find(tp);
        if (it == subscribe_infos_.end()) {
            continue;
        }

        const auto size = it->second.size();
        for (size_t i = 0; i < size; ++i) {
            if (it->second[i] != ptr) {
                continue;
            }

            if (i != size - 1) {
                it->second[i] = std::move(it->second[size - 1]);
            }
            it->second.pop_back();
            break;
        }
    }

    ptr->service = nullptr;
    ptr->socket = 0;
}

simple::task<> local_listener::socket_check(const socket_ptr& ptr) const {
    constexpr int64_t auto_close_session = 180;
    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_int_distribution dis(auto_close_session / 3, auto_close_session * 4 / 3);
    const auto socket = ptr->socket;
    auto& network = simple::network::instance();

    co_await simple::sleep_for(std::chrono::seconds(dis(engine)));
    while (ptr->socket > 0) {
        if (time(nullptr) - ptr->last_recv > auto_close_session) {
            simple::warn("[{}] close socket:{} by check", service_.name(), socket);
            network.close(socket);
            break;
        }

        co_await simple::sleep_for(std::chrono::seconds(dis(engine)));
    }
}

void local_listener::forward_message(const socket_ptr& ptr, uint16_t id, uint64_t session,
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

void local_listener::service_subscribe(const socket_ptr& ptr, uint64_t session, const simple::memory_buffer& buffer) {
    game::s_service_subscribe_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("local_listener socket {} parse s_service_subscribe_req fail", ptr->socket);
        return simple::network::instance().close(ptr->socket);
    }

    game::s_service_subscribe_ack ack;
    auto& result = *ack.mutable_result();
    // 验证连接是否已经注册
    if (!ptr->service || ptr->service->socket != ptr->socket) {
        result.set_ec(game::ec_system);
        return send(ptr->socket, game::id_s_service_subscribe_ack, session, ack);
    }

    const auto tp = static_cast<uint16_t>(req.tp());
    auto& subscribe_info = subscribe_infos_[tp];
    if (std::ranges::find(subscribe_info, ptr) == subscribe_info.end()) {
        subscribe_info.emplace_back(ptr);
    }
    if (std::ranges::find(ptr->subscribe, tp) == ptr->subscribe.end()) {
        ptr->subscribe.emplace_back(tp);
    }

    // 订阅特定类型的所有服务在线状态
    auto* services = service_.router().call<const std::vector<service_info*>*>("find_service_type", tp);
    if (services) {
        for (auto* s : *services) {
            s->to_proto(*ack.add_services());
        }
    }

    result.set_ec(game::ec_success);
    return send(ptr->socket, game::id_s_service_subscribe_ack, session, ack);
}

void local_listener::service_register(const socket_ptr& ptr, uint64_t session, const simple::memory_buffer& buffer) {
    auto req = std::make_shared<game::s_service_register_req>();
    if (!req->ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("[{}] socket:{} parse s_service_register_req fail", service_.name(), ptr->socket);
        simple::network::instance().close(ptr->socket);
        return;
    }

    simple::co_start([req, ptr, session, this]() -> simple::task<> {
        if (ptr->socket == 0) {
            // 这个连接掉线了，可以不用处理了
            co_return;
        }

        game::s_service_register_ack ack;
        ack.set_gate(service_.id());
        auto& result = *ack.mutable_result();
        auto& info = req->info();
        const auto id = static_cast<uint16_t>(info.id());
        if (id == 0) {
            simple::error("[{}] socket:{} registered service:{} is invalid.", service_.name(), ptr->socket, id);
            result.set_ec(game::ec_system);
            send(ptr->socket, game::id_s_service_register_ack, session, ack);
            co_return;
        }

        if (ptr->service) {
            if (ptr->service->id == id) {
                // 说明之前已经注册成功了
                result.set_ec(game::ec_success);
            } else {
                // 说明之前已经注册成功了其他的服务id
                result.set_ec(game::ec_system);
            }
            send(ptr->socket, game::id_s_service_register_ack, session, ack);
            co_return;
        }

        auto& router = service_.router();
        local_service* service;
        if (auto* service_ptr = router.call<service_info*>("find_service", id); !service_ptr) {
            // 向gate master上传，成功了才继续处理
            game::s_service_info temp = info;
            temp.set_online(false);
            auto timeout = []() -> simple::task<> { co_await simple::sleep_for(std::chrono::seconds(5)); };
            auto temp_task = router.call<std::shared_ptr<simple::task<int32_t>>>("upload_to_master", temp);
            const auto rpc_result = co_await (std::move(*temp_task) || timeout());
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
            service = add_local_service(*req);
        } else {
            service = dynamic_cast<local_service*>(service_ptr);
            if (!service) {
                result.set_ec(game::ec_system);
                send(ptr->socket, game::id_s_service_register_ack, session, ack);
                co_return;
            }
        }

        ptr->service = service;
        service->socket = ptr->socket;
        service->online = true;
        service->update();

        simple::warn("[{}] socket:{} registered service:{} succ.", service_.name(), ptr->socket, id);
        result.set_ec(game::ec_success);
        send(ptr->socket, game::id_s_service_register_ack, session, ack);
    });
}

void local_listener::send(uint32_t socket, uint16_t id, uint64_t session, const google::protobuf::Message& msg) {
    const auto buf = create_net_buffer(id, session, msg);
    auto& network = simple::network::instance();
    network.write(socket, buf);
}

local_service* local_listener::add_local_service(const game::s_service_register_req& req) {
    auto& service = services_.emplace_back();
    service.gate = service_.id();
    auto& info = req.info();
    service.id = info.id();
    service.tp = info.tp();
    service.service = &service_;
    service.channel =
        std::make_unique<simple::shm_channel>(std::to_string(service.gate), std::to_string(service.id), req.channel_size());
    service.start();

    for (auto& shm_info : req.shm()) {
        const auto& name = shm_info.name();
        if (!service_shm_map_.contains(name)) {
            service_shm_map_.emplace(name, simple::shm(name, shm_info.size()));
        }
    }

    auto& router = service_.router();
    router.call("emplace_service", dynamic_cast<service_info*>(&service));

    return &service;
}

std::vector<service_info*> local_listener::local_services() {
    std::vector<service_info*> result;
    for (auto& s : services_) {
        result.emplace_back(&s);
    }
    return result;
}
