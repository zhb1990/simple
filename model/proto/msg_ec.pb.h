// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: msg_ec.proto

#ifndef GOOGLE_PROTOBUF_INCLUDED_msg_5fec_2eproto
#define GOOGLE_PROTOBUF_INCLUDED_msg_5fec_2eproto

#include <proto.hpp>
#include <limits>
#include <string>

#include <google/protobuf/port_def.inc>
#if PROTOBUF_VERSION < 3021000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers. Please update
#error your headers.
#endif
#if 3021008 < PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers. Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/port_undef.inc>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/metadata_lite.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/repeated_field.h>  // IWYU pragma: export
#include <google/protobuf/extension_set.h>  // IWYU pragma: export
#include <google/protobuf/generated_enum_reflection.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>
#define PROTOBUF_INTERNAL_EXPORT_msg_5fec_2eproto PROTO_API
PROTOBUF_NAMESPACE_OPEN
namespace internal {
class AnyMetadata;
}  // namespace internal
PROTOBUF_NAMESPACE_CLOSE

// Internal implementation detail -- do not use these members.
struct PROTO_API TableStruct_msg_5fec_2eproto {
  static const uint32_t offsets[];
};
PROTO_API extern const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable descriptor_table_msg_5fec_2eproto;
PROTOBUF_NAMESPACE_OPEN
PROTOBUF_NAMESPACE_CLOSE
namespace game {

enum error_code : int {
  ec_success = 0,
  ec_unknown = 1,
  ec_system = 2,
  error_code_INT_MIN_SENTINEL_DO_NOT_USE_ = std::numeric_limits<int32_t>::min(),
  error_code_INT_MAX_SENTINEL_DO_NOT_USE_ = std::numeric_limits<int32_t>::max()
};
PROTO_API bool error_code_IsValid(int value);
constexpr error_code error_code_MIN = ec_success;
constexpr error_code error_code_MAX = ec_system;
constexpr int error_code_ARRAYSIZE = error_code_MAX + 1;

PROTO_API const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor* error_code_descriptor();
template<typename T>
inline const std::string& error_code_Name(T enum_t_value) {
  static_assert(::std::is_same<T, error_code>::value ||
    ::std::is_integral<T>::value,
    "Incorrect type passed to function error_code_Name.");
  return ::PROTOBUF_NAMESPACE_ID::internal::NameOfEnum(
    error_code_descriptor(), enum_t_value);
}
inline bool error_code_Parse(
    ::PROTOBUF_NAMESPACE_ID::ConstStringParam name, error_code* value) {
  return ::PROTOBUF_NAMESPACE_ID::internal::ParseNamedEnum<error_code>(
    error_code_descriptor(), name, value);
}
// ===================================================================


// ===================================================================


// ===================================================================

#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)

}  // namespace game

PROTOBUF_NAMESPACE_OPEN

template <> struct is_proto_enum< ::game::error_code> : ::std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor< ::game::error_code>() {
  return ::game::error_code_descriptor();
}

PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)

#include <google/protobuf/port_undef.inc>
#endif  // GOOGLE_PROTOBUF_INCLUDED_GOOGLE_PROTOBUF_INCLUDED_msg_5fec_2eproto
