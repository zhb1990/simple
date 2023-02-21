#include "login.h"

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

login::login(const simple::toml_value_t* value) {
    if (!value->is_table()) {
        throw std::logic_error("proxy need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("center"); it != args.end() && it->second.is_integer()) {
        center_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("proxy need center id");
    }

    if (const auto it = args.find("gate"); it != args.end()) {
        gate_connector_ = std::make_shared<gate_connector>(
            *this, &it->second, game::st_login, gate_connector::fn_on_register{},
            [this](uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
                return forward_shm(from, session, id, buffer);
            });
    } else {
        throw std::logic_error("proxy need listen port");
    }
}

simple::task<> login::awake() {
    gate_connector_->start();
    co_return;
}

void login::forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
    // 只有注册登录验证，这里直接向center创建下账号或者验证下密码
}

SIMPLE_SERVICE_API simple::service_base* login_create(const simple::toml_value_t* value) { return new login(value); }

SIMPLE_SERVICE_API void login_release(const simple::service_base* t) { delete t; }
