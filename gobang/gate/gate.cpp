#include "gate.h"

#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <proto_utils.h>

#include <headers.hpp>
#include <simple/coro/co_start.hpp>

#include "local_listener.h"
#include "master_connector.h"
#include "remote_connector.h"
#include "remote_listener.h"

gate::gate(const simple::toml_value_t* value) {
    if (!value->is_table()) {
        throw std::logic_error("gate need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("master_address"); it != args.end() && it->second.is_string()) {
        master_address_ = it->second.as_string();
    } else {
        throw std::logic_error("gate need master address");
    }

    if (const auto it = args.find("local_port"); it != args.end() && it->second.is_integer()) {
        local_port_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("gate need local listen port");
    }

    if (const auto it = args.find("remote_port"); it != args.end() && it->second.is_integer()) {
        remote_port_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("gate need remote listen port");
    }

    if (const auto it = args.find("remote_hosts"); it != args.end() && it->second.is_array()) {
        auto& arr = it->second.as_array();
        for (auto& host : arr) {
            if (host.is_string()) {
                remote_hosts_.emplace_back(host.as_string());
            }
        }
    }

    if (remote_hosts_.empty()) {
        throw std::logic_error("gate need remote hosts");
    }

    master_connector_ = std::make_shared<master_connector>(*this);
    remote_listener_ = std::make_shared<remote_listener>(*this);
    local_listener_ = std::make_shared<local_listener>(*this);
}

simple::task<> gate::awake() {
    master_connector_->start();
    co_await remote_listener_->start();
    co_await local_listener_->start();
}

void gate::forward(game::s_gate_forward_brd brd) {
    const auto dest = static_cast<uint16_t>(brd.to());
    if (dest == 0) {
        return;
    }

    const auto it = services_.find(dest);
    if (it == services_.end()) {
        return delay_forward(dest, std::move(brd));
    }

    if (it->gate != this->id()) {
        auto ptr = create_net_buffer(game::id_s_gate_forward_brd, 0, brd);
        it->remote->connector->send(ptr);
    } else {
        simple::memory_buffer temp;
        shm_header header{static_cast<uint16_t>(brd.from()), dest, static_cast<uint16_t>(brd.id()), 0, brd.session()};
        const auto& data = brd.data();
        temp.reserve(sizeof(header) + data.size());
        temp.append(&header, sizeof(header));
        temp.append(data.data(), data.size());
        it->local->write(temp.begin_read(), static_cast<uint32_t>(temp.readable()));
    }
}

static void init_forward_brd(game::s_gate_forward_brd& brd, const shm_header& header, const std::string_view& data) {
    brd.set_from(header.from);
    brd.set_to(header.to);
    brd.set_id(header.id);
    brd.set_session(header.session);
    *brd.mutable_data() = data;
}

static simple::memory_buffer_ptr create_forward_buffer(const shm_header& header, const std::string_view& data) {
    game::s_gate_forward_brd brd;
    init_forward_brd(brd, header, data);
    return create_net_buffer(game::id_s_gate_forward_brd, 0, brd);
}

void gate::forward(const std::string_view& message) {
    const shm_header& header = *reinterpret_cast<const shm_header*>(message.data());
    const auto dest = header.to;
    if (dest == 0) {
        return;
    }

    const auto it = services_.find(dest);
    if (it == services_.end()) {
        const auto data = message.substr(sizeof(header));
        game::s_gate_forward_brd brd;
        init_forward_brd(brd, header, data);
        return delay_forward(dest, std::move(brd));
    }

    if (it->gate != id()) {
        const auto data = message.substr(sizeof(header));
        auto ptr = create_forward_buffer(header, data);
        it->remote->connector->send(ptr);
    } else {
        it->local->write(message.data(), static_cast<uint32_t>(message.size()));
    }
}

void gate::forward(const service_data* dest) {
    const auto it = send_queues_.find(dest->id);
    if (it == send_queues_.end()) {
        return;
    }

    for (auto& brd : it->second) {
        if (dest->gate != this->id()) {
            auto ptr = create_net_buffer(game::id_s_gate_forward_brd, 0, *brd);
            dest->remote->connector->send(ptr);
        } else {
            simple::memory_buffer temp;
            shm_header header{static_cast<uint16_t>(brd->from()), dest->id, static_cast<uint16_t>(brd->id()), 0,
                              brd->session()};
            const auto& data = brd->data();
            temp.reserve(sizeof(header) + data.size());
            temp.append(&header, sizeof(header));
            temp.append(data.data(), data.size());
            dest->local->write(temp.begin_read(), static_cast<uint32_t>(temp.readable()));
        }
    }

    send_queues_.erase(it);
}

void gate::delay_forward(uint16_t dest, game::s_gate_forward_brd brd) {
    auto& queue = send_queues_[dest];
    queue.emplace_back(std::make_shared<game::s_gate_forward_brd>(std::move(brd)));
    constexpr size_t max_send_queue_size = 500;
    if (queue.size() > max_send_queue_size) {
        queue.pop_front();
    }
}

service_type_info& gate::get_service_type_info(uint16_t tp) { return service_type_infos_[tp]; }

simple::task<int32_t> gate::upload_to_master(const game::s_service_info& info) {
    return master_connector_->upload_to_master(info);
}

const service_data* gate::add_local_service(const game::s_service_register_req& req) {
    auto& info = req.info();
    const auto service = static_cast<uint16_t>(info.id());
    auto it_service = services_.find(service);
    if (it_service != services_.end()) {
        // 之前已经注册过了，这里只改变下在线状态
        it_service->online = false;
        return &*it_service;
    }

    const auto tp = static_cast<uint16_t>(info.tp());
    service_data temp;
    temp.gate = id();
    temp.id = service;
    temp.tp = tp;
    auto& channel = local_channels_.emplace_back(
        std::make_unique<channel_cached>(std::to_string(temp.gate), std::to_string(service), req.channel_size()));
    temp.local = channel.get();
    std::tie(it_service, std::ignore) = services_.emplace(temp);
    auto* service_ptr = &*it_service;
    local_services_.emplace_back(service_ptr);

    auto& service_type = service_type_infos_[tp];
    service_type.services.emplace_back(service_ptr);

    for (auto& shm_info : req.shm()) {
        const auto& name = shm_info.name();
        if (!service_shm_map_.contains(name)) {
            service_shm_map_.emplace(name, simple::shm(name, shm_info.size()));
        }
    }

    forward(service_ptr);

    channel->auto_write();

    simple::co_start([&channel, this]() -> simple::task<> {
        simple::memory_buffer buf;
        for (;;) {
            co_await channel->channel.read(buf);
            forward(std::string_view(buf));
        }
    });

    return service_ptr;
}

simple::task<> gate::local_service_online_changed(uint16_t service, bool online) {
    auto it_service = services_.find(service);
    if (it_service == services_.end()) {
        // 不可能走到这里
        co_return;
    }

    it_service->online = online;
    local_listener_->publish(&*it_service);

    game::s_service_info temp;
    it_service->to_proto(temp);
    co_await upload_to_master(temp);
}

void gate::add_remote_gate(const game::s_gate_info& gate_info) {
    const auto gate_id = gate_info.id();
    auto it_remote = remote_gates_.find(gate_id);
    if (it_remote == remote_gates_.end()) {
        remote_gate temp;
        temp.id = gate_id;
        std::tie(it_remote, std::ignore) = remote_gates_.emplace(temp);
        it_remote->connector = std::make_shared<remote_connector>(&*it_remote, *this);
        it_remote->connector->start();
    }

    it_remote->addresses.clear();
    for (auto& address : gate_info.addresses()) {
        auto& str = it_remote->addresses.emplace_back();
        str.append(address.host());
        str.push_back(',');
        str.append(address.port());
    }

    for (auto& s : gate_info.services()) {
        auto* data = add_remote_service(*it_remote, s);
        local_listener_->publish(data);
    }
}

const service_data* gate::add_remote_service(const remote_gate& remote, const game::s_service_info& info) {
    const auto service = static_cast<uint16_t>(info.id());
    auto it_service = services_.find(service);
    if (it_service != services_.end()) {
        // 之前已经注册过了，这里只改变下在线状态
        it_service->online = false;
        return &*it_service;
    }

    const auto tp = static_cast<uint16_t>(info.tp());
    service_data temp;
    temp.gate = remote.id;
    temp.id = service;
    temp.tp = tp;
    temp.remote = &remote;
    std::tie(it_service, std::ignore) = services_.emplace(temp);
    auto* service_ptr = &*it_service;
    remote.services.emplace_back(service_ptr);

    auto& service_type = service_type_infos_[tp];
    service_type.services.emplace_back(service_ptr);

    forward(service_ptr);

    return service_ptr;
}

void service_data::to_proto(game::s_service_info& info) const noexcept {
    info.set_id(id);
    info.set_tp(static_cast<game::service_type>(tp));
    info.set_online(online);
}

void channel_cached::write(const void* buf, uint32_t len) {
    // 发送队列中有数据，或者写入失败说明共享内存写满了
    if (!send_queue.empty() || !channel.try_write(buf, len)) {
        // 放入发送队列
        send_queue.emplace_back(std::make_shared<simple::memory_buffer>(buf, len));
        cv_send_queue.notify_all();
    }
}

void channel_cached::auto_write() {
    simple::co_start([this]() -> simple::task<> {
        for (;;) {
            if (send_queue.empty()) {
                co_await cv_send_queue.wait();
                continue;
            }

            const auto ptr = send_queue.front();
            co_await channel.write(ptr->begin_read(), static_cast<uint32_t>(ptr->readable()));
            send_queue.pop_front();
        }
    });
}

SIMPLE_SERVICE_API simple::service_base* gate_create(const simple::toml_value_t* value) { return new gate(value); }

SIMPLE_SERVICE_API void gate_release(const simple::service_base* t) { delete t; }
