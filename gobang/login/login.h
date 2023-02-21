#pragma once

#include <simple/web/websocket.h>

#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>

class gate_connector;

namespace game {
class s_service_info;
}

class login final : public simple::service_base {
  public:
    explicit login(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(login)

    ~login() noexcept override = default;

    simple::task<> awake() override;

  private:
    void forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

    // 连接gate
    std::shared_ptr<gate_connector> gate_connector_;
    uint16_t center_;
};
