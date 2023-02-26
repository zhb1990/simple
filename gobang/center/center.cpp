#include "center.h"

#include <gate_connector.h>
#include <google/protobuf/util/json_util.h>
#include <msg_client.pb.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>
#include <stdexcept>

center::center(const simple::toml_value_t* value) {
    if (!value->is_table()) {
        throw std::logic_error("center need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("gate"); it != args.end()) {
        gate_connector_ = std::make_shared<gate_connector>(
            *this, &it->second, game::st_proxy, [this] { return on_register_to_gate(); },
            [this](uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
                return forward_shm(from, session, id, buffer);
            });
    } else {
        throw std::logic_error("center need gate config");
    }
}

simple::task<> center::awake() {
    gate_connector_->start();
    co_return;
}

simple::task<> center::on_register_to_gate() { co_return; }

void center::forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {}

SIMPLE_SERVICE_API simple::service_base* center_create(const simple::toml_value_t* value) { return new center(value); }

SIMPLE_SERVICE_API void center_release(const simple::service_base* t) { delete t; }
