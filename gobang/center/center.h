#pragma once

#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>

class gate_connector;

class center final : public simple::service_base {
  public:
    explicit center(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(center)

    ~center() noexcept override = default;

    simple::task<> awake() override;

  private:
    simple::task<> on_register_to_gate();

    void forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

	// 处理注册、登录


    // 连接gate
    std::shared_ptr<gate_connector> gate_connector_;
    simple::memory_buffer temp_buffer_;
};
