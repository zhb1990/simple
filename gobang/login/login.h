#pragma once

#include <simple/web/websocket.h>

#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>

#include <msg_client.pb.h>

class gate_connector;

class login final : public simple::service_base {
  public:
    explicit login(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(login)

    ~login() noexcept override = default;

    simple::task<> awake() override;

  private:
    void forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

    simple::task<> client_login(uint16_t from, uint32_t socket, uint64_t session, const game::login_req& req);

    // 内部登录，注册或者校验密码，返回玩家userid
    simple::task<int32_t> internal_login(const std::string& account, const std::string& password, game::ack_result& result);

	simple::task<uint16_t> internal_get_logic(int32_t userid, game::ack_result& result);

    simple::task<> internal_login_logic(int32_t userid, uint16_t logic, uint16_t gate, uint32_t socket, game::login_ack& ack);

    // 连接gate
    std::shared_ptr<gate_connector> gate_connector_;
    uint16_t center_;
    uint16_t logic_master_;
    simple::memory_buffer temp_buffer_;
};
