#pragma once
#include <cstdint>
#include <string_view>

namespace game {
class s_service_info;
}

namespace simple {
class service;
}

struct service_info {
    virtual ~service_info() noexcept = default;

    // 服务id
    uint16_t id{0};
	// 服务类型
    uint16_t tp{0};
	// 所属gate的服务id
    uint16_t gate{0};
    // 是否在线
    bool online{false};
    // gate服务的指针
    simple::service* service{nullptr};

    void to_proto(game::s_service_info& info) const noexcept;

    virtual void write(const std::string_view& message) = 0;

    void update();
};

struct service_update_event {
    service_info* info{nullptr};
};

struct forward_message_event {
    std::string_view strv;
};
