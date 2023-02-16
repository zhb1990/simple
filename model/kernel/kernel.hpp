#pragma once

#if defined(KERNEL_DLL_EXPORT)
#define KERNEL_API __declspec(dllexport)
#elif defined(KERNEL_DLL_IMPORT)
#define KERNEL_API __declspec(dllimport)
#elif defined(KERNEL_LIB_VISIBILITY) && defined(__GNUC__) && (__GNUC__ >= 4)
#define KERNEL_API __attribute__((visibility("default")))
#else
#define KERNEL_API  // NOLINT(clang-diagnostic-unused-macros)
#endif              // defined(SIMPLE_DLL_EXPORT)
