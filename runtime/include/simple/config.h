#pragma once

#include <cstddef>

#if defined(DS_DLL_EXPORT)
#define DS_API __declspec(dllexport)
#elif defined(DS_DLL_IMPORT)
#define DS_API __declspec(dllimport)
#elif defined(DS_LIB_VISIBILITY) && defined(__GNUC__) && (__GNUC__ >= 4)
#define DS_API __attribute__((visibility("default")))
#else
#define DS_API
#endif  // defined(DS_DLL_EXPORT)

#if defined(powerpc) || defined(__powerpc__) || defined(__ppc__)
inline constexpr auto ds_cache_line_bytes = 128
#else
inline constexpr auto ds_cache_line_bytes = 64;
#endif

// clang-format off

#define DS_CONVERT(ptr, type, member) \
    ((ptr) != nullptr ? reinterpret_cast<type*>(reinterpret_cast<char*>(ptr) - offsetof(type, member)) : nullptr) /*NOLINT(bugprone-macro-parentheses)*/


#define DS_CONVERT_C(ptr, type, member) \
    ((ptr) != nullptr ? reinterpret_cast<const type*>(reinterpret_cast<const char*>(ptr) - offsetof(type, member)) : nullptr)

// clang-format on

#define DS_MARCO_EXPAND(...) __VA_ARGS__

#define DS_NON_COPYABLE(type)                                                     \
    type(const type&) = delete;                                                   \
    type(type&&) = delete;                 /*NOLINT(bugprone-macro-parentheses)*/ \
    type& operator=(const type&) = delete; /*NOLINT(bugprone-macro-parentheses)*/ \
    type& operator=(type&&) = delete;      /*NOLINT(bugprone-macro-parentheses)*/

#define DS_COPYABLE_DEFAULT(type)                                                      \
    type(const type&) = default;                                                       \
    type(type&&) noexcept = default;            /*NOLINT(bugprone-macro-parentheses)*/ \
    type& operator=(const type&) = default;     /*NOLINT(bugprone-macro-parentheses)*/ \
    type& operator=(type&&) noexcept = default; /*NOLINT(bugprone-macro-parentheses)*/

#if defined(WINDOWS_USED_UTF8) || !defined(_WIN32)
#define OS_ENABLE_UTF8 1
#endif
