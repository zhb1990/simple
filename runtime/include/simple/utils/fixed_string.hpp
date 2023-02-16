#pragma clang diagnostic push
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma once
#include <algorithm>
#include <string_view>

namespace simple {
template <size_t N>
struct fixed_string {
    char str[N]{};
    constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, str); }
    constexpr operator std::string_view() const noexcept { return {str, N - 1}; }
};
}  // namespace simple

#pragma clang diagnostic pop
