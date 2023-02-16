#pragma once
#include <simple/config.h>

#include <functional>
#include <memory>

namespace simple {

class cancellation_state;
class cancellation_token;

class cancellation_registration {
  public:
    DS_API cancellation_registration(const cancellation_token& token, std::function<void()>&& callback);

    DS_API ~cancellation_registration() noexcept;

    DS_NON_COPYABLE(cancellation_registration)

  private:
    friend class cancellation_state;

    cancellation_registration* next_;
    cancellation_registration* prev_;

    std::function<void()> callback_;
    std::shared_ptr<cancellation_state> state_;
};

}  // namespace simple
