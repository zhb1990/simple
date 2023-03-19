#pragma once
#include <simple/config.h>

#include <cstdint>
#include <simple/application/call_router.hpp>
#include <simple/application/event_system.hpp>
#include <simple/coro/task.hpp>
#include <simple/utils/toml_types.hpp>
#include <string>
#include <type_traits>

namespace simple {

class application;

class service {
  public:
    service() = default;

    SIMPLE_NON_COPYABLE(service)

    virtual ~service() noexcept = default;

    virtual task<> awake() = 0;

    virtual task<> update() { co_return; }

    bool need_update() noexcept {
        if (interval_ == 0) {
            return false;
        }

        if (++current_ < interval_) {
            return false;
        }

        current_ = 0;
        return true;
    }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] const std::string& type() const noexcept { return type_; }

    [[nodiscard]] uint32_t order() const noexcept { return order_; }

    [[nodiscard]] uint16_t id() const noexcept { return id_; }

    auto& events() noexcept { return events_; }

    auto& router() noexcept { return router_; }

  protected:
    friend class application;
    std::string name_;
    std::string type_;
    uint32_t order_{0};
    uint16_t id_{0};
    uint64_t current_{0};
    uint64_t interval_{0};

    event_system events_;
    call_router router_;
};

using service_create_t = std::add_pointer_t<service*(const toml_value_t*)>;

using service_release_t = std::add_pointer_t<void(const service*)>;

}  // namespace simple

#if defined(_WIN32)
#define SIMPLE_SERVICE_API extern "C" __declspec(dllexport)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define SIMPLE_SERVICE_API extern "C" __attribute__((visibility("default")))
#else
#define SIMPLE_SERVICE_API extern "C"
#endif
