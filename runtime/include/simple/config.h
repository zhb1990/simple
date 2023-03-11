#pragma once

#include <cstddef>

#if defined(SIMPLE_DLL_EXPORT)
#define SIMPLE_API __declspec(dllexport)
#elif defined(SIMPLE_DLL_IMPORT)
#define SIMPLE_API __declspec(dllimport)
#elif defined(SIMPLE_LIB_VISIBILITY) && defined(__GNUC__) && (__GNUC__ >= 4)
#define SIMPLE_API __attribute__((visibility("default")))
#else
#define SIMPLE_API
#endif  // defined(SIMPLE_DLL_EXPORT)

namespace simple {
#if defined(powerpc) || defined(__powerpc__) || defined(__ppc__)
inline constexpr auto simple_cache_line_bytes = 128
#else
inline constexpr auto simple_cache_line_bytes = 64;
#endif
}  // namespace simple

#define SIMPLE_NON_COPYABLE(type)                                                 \
    type(const type&) = delete;                                                   \
    type(type&&) = delete;                 /*NOLINT(bugprone-macro-parentheses)*/ \
    type& operator=(const type&) = delete; /*NOLINT(bugprone-macro-parentheses)*/ \
    type& operator=(type&&) = delete;      /*NOLINT(bugprone-macro-parentheses)*/

#define SIMPLE_COPYABLE_DEFAULT(type)                                                  \
    type(const type&) = default;                                                       \
    type(type&&) noexcept = default;            /*NOLINT(bugprone-macro-parentheses)*/ \
    type& operator=(const type&) = default;     /*NOLINT(bugprone-macro-parentheses)*/ \
    type& operator=(type&&) noexcept = default; /*NOLINT(bugprone-macro-parentheses)*/

#if defined(WINDOWS_USED_UTF8) || !defined(_WIN32)
#define OS_ENABLE_UTF8 1
#endif
