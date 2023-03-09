#pragma once

#include <coroutine>

namespace simple {

struct hash {
    template <typename Promise>
    size_t operator()(const std::coroutine_handle<Promise>& handle) const noexcept {
        return std::hash<std::coroutine_handle<Promise>>{}(handle);
    }
};

}  // namespace simple
