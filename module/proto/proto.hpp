#pragma once

#if defined(PROTO_DLL_EXPORT)
#define PROTO_API __declspec(dllexport)
#elif defined(PROTO_DLL_IMPORT)
#define PROTO_API __declspec(dllimport)
#elif defined(PROTO_LIB_VISIBILITY) && defined(__GNUC__) && (__GNUC__ >= 4)
#define PROTO_API __attribute__((visibility("default")))
#else
#define PROTO_API  // NOLINT(clang-diagnostic-unused-macros)
#endif             // defined(SIMPLE_DLL_EXPORT)
