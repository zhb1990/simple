#include "service_info.h"

#include <msg_server.pb.h>

#include <simple/application/service.hpp>

void service_info::to_proto(game::s_service_info& info) const noexcept {
    info.set_id(id);
    info.set_tp(static_cast<game::service_type>(tp));
    info.set_online(online);
}

void service_info::update() {
    if (service) {
        service->events().fire_event(service_update_event{this});
    }
}
