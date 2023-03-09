#pragma once

#include <exception>

namespace simple {

class multiple_exceptions final : public std::exception {
  public:
    explicit multiple_exceptions(std::exception_ptr first) noexcept : first_(first) {}

    const char* what() const noexcept override { return "multiple_exceptions"; }

    std::exception_ptr first_exception() const { return first_; }

  private:
    std::exception_ptr first_;
};

}  // namespace simple
