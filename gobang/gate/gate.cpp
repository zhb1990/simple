#include "gate.h"

#include <headers.hpp>

#include "local_listener.h"
#include "master_connector.h"
#include "remote_listener.h"

gate::gate(const simple::toml_value_t* value) {
    if (!value->is_table()) {
        throw std::logic_error("gate need args");
    }

    auto& table = value->as_table();
    master_connector_ = std::make_shared<master_connector>(*this, table);
    remote_listener_ = std::make_shared<remote_listener>(*this, table);
    local_listener_ = std::make_shared<local_listener>(*this, table);

    forward_message_ = events_.register_handler<forward_message_event>(&gate::forward, this);
    router_.register_call("find_service", &gate::find_service, this);
    router_.register_call("emplace_service", &gate::emplace_service, this);
    router_.register_call("find_service_type", &gate::find_service_type, this);
}

simple::task<> gate::awake() {
    master_connector_->start();
    co_await remote_listener_->start();
    co_await local_listener_->start();
}

service_info* gate::find_service(uint16_t id) {
    const auto it = services_.find(id);
    if (it == services_.end()) {
        return nullptr;
    }

    return it->second;
}

void gate::emplace_service(service_info* info) {
    services_.emplace(info->id, info);
    service_types_[info->tp].emplace_back(info);
    if (const auto it = send_queues_.find(info->id); it != send_queues_.end()) {
        for (const auto& buf : it->second) {
            info->write(std::string_view(buf));
        }
        send_queues_.erase(it);
    }
}

const std::vector<service_info*>* gate::find_service_type(uint16_t tp) {
    const auto it = service_types_.find(tp);
    if (it == service_types_.end()) {
        return nullptr;
    }

    return &it->second;
}

void gate::forward(const forward_message_event& ev) {
    if (ev.strv.size() < sizeof(forward_part)) {
        return;
    }

    auto* part = reinterpret_cast<const forward_part*>(ev.strv.data());
    const auto dest = part->to;
    auto* service = find_service(dest);
    if (!service) {
        auto& queue = send_queues_[dest];
        queue.emplace_back(ev.strv.data(), ev.strv.size());
        return;
    }

    service->write(ev.strv);
}

SIMPLE_SERVICE_API simple::service* gate_create(const simple::toml_value_t* value) { return new gate(value); }

SIMPLE_SERVICE_API void gate_release(const simple::service* t) { delete t; }
