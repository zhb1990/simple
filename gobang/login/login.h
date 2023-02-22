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

    simple::task<> client_login(uint16_t from, uint32_t socket, const game::login_req& req);

    // 连接gate
    std::shared_ptr<gate_connector> gate_connector_;
    uint16_t center_;
    uint16_t logic_master_;
    simple::memory_buffer temp_buffer_;
};
