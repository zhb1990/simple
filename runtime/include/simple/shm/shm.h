#pragma once
#include <simple/config.h>

#include <cstdint>
#include <string_view>
#include <system_error>

namespace simple {

struct shm_impl;

class shm {
  public:
    DS_API shm(std::string_view name, size_t size);

    DS_API ~shm() noexcept;

    shm(const shm&) = delete;

    DS_API shm(shm&& other) noexcept;

    shm& operator=(const shm&) = delete;

    DS_API shm& operator=(shm&& other) noexcept;

    [[nodiscard]] auto data() const noexcept { return data_; }

    [[nodiscard]] auto size() const noexcept { return size_; }

    DS_API std::error_code get_error_code();

    [[nodiscard]] auto is_create() const noexcept { return is_create_; }

  private:
    shm_impl* impl_{nullptr};
    void* data_{nullptr};
    size_t size_{0};
    bool is_create_{false};
};

}  // namespace simple
