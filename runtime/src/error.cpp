#include <simple/error.h>

#include <string>

namespace simple {

class socket_category final : public std::error_category {
  public:
    [[nodiscard]] const char* name() const noexcept override { return "simple.socket"; }

    [[nodiscard]] std::string message(int value) const override {
        switch (static_cast<socket_errors>(value)) {
            case socket_errors::kcp_check_failed:
                return "kcp check failed";
            case socket_errors::kcp_heartbeat_timeout:
                return "kcp heartbeat timeout";
            case socket_errors::kcp_protocol_error:
                return "kcp protocol error";
            case socket_errors::initiative_disconnect:
                return "application initiative to disconnect";
            default:  // NOLINT(clang-diagnostic-covered-switch-default)
                return "simple.socket error";
        }
    }
};

const std::error_category& get_socket_category() {
    static socket_category category;
    return category;
}

class coro_category final : public std::error_category {
  public:
    [[nodiscard]] const char* name() const noexcept override { return "simple.coro"; }

    [[nodiscard]] std::string message(int value) const override {
        switch (static_cast<coro_errors>(value)) {
            case coro_errors::canceled:
                return "cancellation requested";
            case coro_errors::broken_promise:
                return "broken promise";
            case coro_errors::invalid_action:
                return "coro invalid action";
            default:  // NOLINT(clang-diagnostic-covered-switch-default)
                return "simple.coro error";
        }
    }
};

const std::error_category& get_coro_category() {
    static coro_category category;
    return category;
}

}  // namespace simple
