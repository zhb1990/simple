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
        throw std::logic_error("login need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("center"); it != args.end() && it->second.is_integer()) {
        center_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("login need center id");
    }

    if (const auto it = args.find("logic_master"); it != args.end() && it->second.is_integer()) {
        logic_master_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("login need logic_master id");
    }

    if (const auto it = args.find("gate"); it != args.end()) {
        gate_connector_ = std::make_shared<gate_connector>(
            *this, &it->second, game::st_login, gate_connector::fn_on_register{},
            [this](uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
                return forward_shm(from, session, id, buffer);
            });
    } else {
        throw std::logic_error("login need gate config");
    }
}

simple::task<> login::awake() {
    gate_connector_->start();
    co_return;
}

void login::forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
    // 只有注册登录验证
    if (id != game::id_s_client_forward_brd) {
        return;
    }

    game::s_client_forward_brd brd;
    if (!brd.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        return;
    }

    if (brd.id() != game::id_login_req) {
        return;
    }

    game::login_req req;
    if (const auto& data = brd.data(); !req.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        return;
    }

    simple::co_start([this, from, socket = brd.socket(), req = std::move(req), session = brd.session()] {
        return client_login(from, socket, session, req);
    });
}

static auto rpc_timeout = []() -> simple::task<> { co_await simple::sleep_for(std::chrono::seconds{10}); };

simple::task<> login::client_login(uint16_t from, uint32_t socket, uint64_t session, const game::login_req& req) {
    game::login_ack ack;
    auto& result = *ack.mutable_result();
    uint16_t logic = 0;
    try {
        do {
            // 不存在sdk，这里直接向center创建下账号或者验证下密码
            int32_t userid = co_await internal_login(req.account(), req.password(), result);
            if (result.ec() != game::ec_success) {
                break;
            }
            ack.set_userid(userid);

            // 校验完后向 逻辑管理服去请求 逻辑服id
            logic = co_await internal_get_logic(userid, result);
            if (result.ec() != game::ec_success) {
                break;
            }

            // 向逻辑服请求玩家的数据
            co_await internal_login_logic(userid, logic, from, socket, ack);
        } while (false);
    } catch (std::exception& e) {
        simple::error("[{}] gate:{} socket:{} login exception {}", name(), from, socket, ERROR_CODE_MESSAGE(e.what()));
        ack.mutable_result()->set_ec(game::ec_system);
    }

    game::s_client_forward_brd brd;
    brd.set_gate(from);
    brd.set_socket(socket);
    brd.set_id(game::id_login_ack);
    brd.set_session(session);
    brd.set_logic(logic);
    temp_buffer_.clear();
    const auto len = ack.ByteSizeLong();
    temp_buffer_.reserve(len);
    ack.SerializePartialToArray(temp_buffer_.begin_write(), static_cast<int>(temp_buffer_.writable()));
    temp_buffer_.written(len);
    brd.set_data(temp_buffer_.begin_read(), temp_buffer_.readable());
    gate_connector_->write(from, 0, game::id_s_client_forward_brd, brd);
}

simple::task<int32_t> login::internal_login(const std::string& account, const std::string& password, game::ack_result& result) {
    game::s_login_req req;
    req.set_account(account);
    req.set_password(req.password());
    auto value = co_await (gate_connector_->call<game::s_login_ack>(center_, game::id_s_login_req, req) || rpc_timeout());
    if (value.index() != 0) {
        // 超时了
        simple::error("[{}] internal_login {}, {} timeout", name(), account, password);
        result.set_ec(game::ec_timeout);
        co_return 0;
    }

    const auto& ack = std::get<0>(value);
    if (const auto ec = ack.result().ec(); ec != game::ec_success) {
        simple::warn("[{}] internal_login {}, {}, ec:{}", name(), account, password, ec);
        result.set_ec(ec);
        co_return 0;
    }

    co_return ack.userid();
}

simple::task<uint16_t> login::internal_get_logic(int32_t userid, game::ack_result& result) {
    game::s_get_logic_req req;
    req.set_userid(userid);
    auto value =
        co_await (gate_connector_->call<game::s_get_logic_ack>(logic_master_, game::id_s_get_logic_req, req) || rpc_timeout());
    if (value.index() != 0) {
        // 超时了
        simple::error("[{}] userid:{} get logic timeout", name(), userid);
        result.set_ec(game::ec_timeout);
        co_return 0;
    }

    const auto& ack = std::get<0>(value);
    if (const auto ec = ack.result().ec(); ec != game::ec_success) {
        simple::warn("[{}] userid:{} get logic ec:{}", name(), userid, ec);
        result.set_ec(ec);
        co_return 0;
    }

    co_return ack.logic();
}

simple::task<> login::internal_login_logic(int32_t userid, uint16_t logic, uint16_t gate, uint32_t socket,
                                           game::login_ack& ack) {
    game::s_login_logic_req internal_req;
    internal_req.set_userid(userid);
    internal_req.set_gate(gate);
    internal_req.set_socket(socket);
    auto value = co_await (gate_connector_->call<game::s_login_logic_ack>(logic, game::id_s_login_logic_req, internal_req) ||
                           rpc_timeout());
    if (value.index() != 0) {
        // 超时了
        simple::error("[{}] userid:{} gate:{} socket:{} login logic {} timeout", name(), userid, gate, socket, logic);
        ack.mutable_result()->set_ec(game::ec_timeout);
        co_return;
    }

    const auto& internal_ack = std::get<0>(value);
    if (const auto ec = internal_ack.result().ec(); ec != game::ec_success) {
        simple::warn("[{}] userid:{} gate:{} socket:{} login logic {} ec:{}", name(), userid, gate, socket, logic, ec);
        ack.mutable_result()->set_ec(ec);
        co_return;
    }

    ack.set_room(internal_ack.room());
    ack.set_win_count(internal_ack.win_count());
    ack.set_lose_count(internal_ack.lose_count());
}

SIMPLE_SERVICE_API simple::service_base* login_create(const simple::toml_value_t* value) { return new login(value); }

SIMPLE_SERVICE_API void login_release(const simple::service_base* t) { delete t; }
