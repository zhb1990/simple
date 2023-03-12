#pragma once

#include <simple/config.h>

#include <cstdint>
#include <functional>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace simple {

class event_system;

class event_registration {
  public:
    event_registration(const event_registration& other) = delete;

    event_registration(event_registration&& other) noexcept
        : system_(std::exchange(other.system_, nullptr)),
          handle_(std::exchange(other.handle_, nullptr)),
          type_(std::exchange(other.type_, typeid(void))) {}

    event_registration& operator=(const event_registration& other) = delete;

    event_registration& operator=(event_registration&& other) noexcept {
        if (this != &other) {
            system_ = std::exchange(other.system_, nullptr);
            handle_ = std::exchange(other.handle_, nullptr);
            type_ = std::exchange(other.type_, typeid(void));
        }

        return *this;
    }

    ~event_registration() { deregister(); }

    void deregister();

  private:
    event_registration(event_system* system, const void* handle, const std::type_index& index)
        : system_(system), handle_(handle), type_(index) {}

    friend class event_system;
    event_system* system_{nullptr};
    const void* handle_{nullptr};
    std::type_index type_{typeid(void)};
};

class event_system {
  public:
    template <typename EventType, std::invocable<EventType> Handle>
    event_registration register_handler(Handle&& handle) {
        std::type_index index{typeid(EventType)};
        auto it = handlers_.emplace(
            index, [h = std::forward<Handle>(handle)](const void* ev) { h(*static_cast<const EventType*>(ev)); });
        return {this, &it->second, index};
    }

    template <typename EventType, typename Class, typename Func>
    requires std::invocable<Func Class::*, Class*, EventType>
    event_registration register_handler(Func Class::*func, Class* self) {
        std::type_index index{typeid(EventType)};
        auto it = handlers_.emplace(index, [func, self](const void* ev) { (self->*func)(*static_cast<const EventType*>(ev)); });
        return {this, &it->second, index};
    }

    bool deregister_handler(const event_registration& registration) {
        for (auto [begin, end] = handlers_.equal_range(registration.type_); begin != end; ++begin) {
            if (registration.handle_ == static_cast<const void*>(&begin->second)) {
                handlers_.erase(begin);
                return true;
            }
        }

        return false;
    }

    template <typename EventType>
    void fire_event(const EventType& ev) {
        for (auto [begin, end] = handlers_.equal_range(std::type_index{typeid(EventType)}); begin != end; ++begin) {
            begin->second(&ev);
        }
    }

  protected:
    using handler = std::function<void(const void*)>;
    std::unordered_multimap<std::type_index, handler> handlers_;
};

inline void event_registration::deregister() {
    if (system_ && handle_) {
        system_->deregister_handler(*this);
        handle_ = nullptr;
    }
}

}  // namespace simple
