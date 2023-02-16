#pragma once
#include <memory>
#include <simple/application/service.hpp>
#include <simple/coro/task.hpp>
#include <string>
#include <type_traits>

namespace simple {

class service_base;

class service_dll {
  public:
    service_dll() = default;

    service_dll(const service_dll&) = delete;

    service_dll(service_dll&& other) noexcept { swap(other); }

    ~service_dll() noexcept;

    service_dll& operator=(const service_dll&) = delete;

    service_dll& operator=(service_dll&& other) noexcept {
        if (this != std::addressof(other)) {
            service_dll temp = std::move(*this);
            swap(other);
        }
        return *this;
    }

    void swap(service_dll& other) noexcept {
        std::swap(create_, other.create_);
        std::swap(release_, other.release_);
        std::swap(handle_, other.handle_);
    }

    service_base* create(const toml_value_t*) const;

    void release(const service_base*) const;

  private:
    friend class service_loader;

    // 服务创建函数
    service_create_t create_{nullptr};
    // 服务销毁函数
    service_release_t release_{nullptr};
    void* handle_{nullptr};
};

class service_loader {
    service_loader() = default;

  public:
    DS_NON_COPYABLE(service_loader)

    ~service_loader() noexcept = default;

    static service_loader& instance();

    void add_path(const std::string& utf8_path);

    const service_dll* query(const std::string& name);

  private:
    std::vector<std::string> path_;
    std::unordered_map<std::string, service_dll> service_dll_map_;
};

}  // namespace simple
