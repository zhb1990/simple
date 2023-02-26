#pragma once

#include <google/protobuf/message.h>
#include <simple/coro/cancellation_registration.h>
#include <simple/coro/cancellation_token.h>
#include <simple/coro/scheduler.h>

#include <concepts>
#include <coroutine>
#include <exception>
#include <kernel.hpp>
#include <optional>
#include <string_view>

class rpc_system;

class rpc_awaiter_base {
  public:
    rpc_awaiter_base(uint64_t session, google::protobuf::Message* message, rpc_system* system)
        : session_(session), message_ptr_(message), system_(system) {}

    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle);

    KERNEL_API void check_resume();

    KERNEL_API void parse_message(std::string_view data);

    [[nodiscard]] uint64_t session() const noexcept { return session_; }

  protected:
    friend class rpc_system;

    uint64_t session_;
    google::protobuf::Message* message_ptr_;
    rpc_system* system_;
    std::exception_ptr exception_;
    std::coroutine_handle<> handle_;
    simple::cancellation_token token_;
    std::optional<simple::cancellation_registration> registration_;
};

template <std::derived_from<google::protobuf::Message> Message>
class rpc_awaiter final : public rpc_awaiter_base {
  public:
    rpc_awaiter(uint64_t session, rpc_system* system) : rpc_awaiter_base(session, &message_, system) {}

    rpc_awaiter(const rpc_awaiter& other) : rpc_awaiter_base(other.session_, &message_, other.system_) {}

    rpc_awaiter(rpc_awaiter&&) noexcept = delete;

    ~rpc_awaiter() noexcept = default;

    rpc_awaiter& operator=(const rpc_awaiter&) = delete;

    rpc_awaiter& operator=(rpc_awaiter&&) noexcept = delete;

    Message await_resume() {
        check_resume();
        return std::move(message_);
    }

  private:
    Message message_;
};

class rpc_system {
  public:
    KERNEL_API uint64_t create_session() noexcept;

    KERNEL_API void insert_session(uint64_t session, rpc_awaiter_base* awaiter);

    template <std::derived_from<google::protobuf::Message> Message>
    auto get_awaiter(uint64_t session) {
        return rpc_awaiter<Message>(session, this);
    }

    template <std::derived_from<google::protobuf::Message> Message>
    auto get_awaiter() {
        return rpc_awaiter<Message>(create_session(), this);
    }

    // 取消时调用，仅能在调度线程中使用
    KERNEL_API void wake_up_session(uint64_t session) noexcept;

    // 收到协议数据后调用，仅能在调度线程中使用
    KERNEL_API bool wake_up_session(uint64_t session, std::string_view data) noexcept;

  private:
    std::unordered_map<uint64_t, rpc_awaiter_base*> wait_map_;
    uint32_t sequence_{0};
    uint64_t time_{0};
};

template <typename Promise>
bool rpc_awaiter_base::await_suspend(std::coroutine_handle<Promise> handle) {
    handle_ = handle;
    token_ = handle.promise().get_cancellation_token();

    if (token_.is_cancellation_requested()) {
        return false;
    }

    if (token_.can_be_cancelled()) {
        registration_.emplace(token_, [this] { system_->wake_up_session(session_); });
    }

    system_->insert_session(session_, this);

    return true;
}
