// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: msg_id.proto

#include "msg_id.pb.h"

#include <algorithm>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>

PROTOBUF_PRAGMA_INIT_SEG

namespace _pb = ::PROTOBUF_NAMESPACE_ID;
namespace _pbi = _pb::internal;

namespace game {
}  // namespace game
static const ::_pb::EnumDescriptor* file_level_enum_descriptors_msg_5fid_2eproto[2];
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_msg_5fid_2eproto = nullptr;
const uint32_t TableStruct_msg_5fid_2eproto::offsets[1] = {};
static constexpr ::_pbi::MigrationSchema* schemas = nullptr;
static constexpr ::_pb::Message* const* file_default_instances = nullptr;

const char descriptor_table_protodef_msg_5fid_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n\014msg_id.proto\022\004game*\236\001\n\014message_type\022\017\n"
  "\013msg_c2s_req\020\000\022\020\n\013msg_s2c_ack\020\200 \022\020\n\013msg_"
  "c2s_brd\020\200@\022\020\n\013msg_s2c_brd\020\200`\022\021\n\013msg_s2s_"
  "req\020\200\200\001\022\021\n\013msg_s2s_ack\020\200\240\001\022\021\n\013msg_s2s_br"
  "d\020\200\300\001\022\016\n\010msg_mask\020\200\340\003*\240\003\n\nmessage_id\022\013\n\007"
  "id_none\020\000\022\023\n\rid_s_ping_req\020\201\200\001\022\023\n\rid_s_p"
  "ing_ack\020\201\240\001\022\034\n\026id_s_gate_register_req\020\202\200"
  "\001\022\034\n\026id_s_gate_register_ack\020\202\240\001\022\034\n\026id_s_"
  "gate_register_brd\020\202\300\001\022\037\n\031id_s_service_re"
  "gister_req\020\203\200\001\022\037\n\031id_s_service_register_"
  "ack\020\203\240\001\022\035\n\027id_s_service_update_req\020\204\200\001\022\035"
  "\n\027id_s_service_update_ack\020\204\240\001\022\033\n\025id_s_ga"
  "te_forward_brd\020\205\300\001\022 \n\032id_s_service_subsc"
  "ribe_req\020\206\200\001\022 \n\032id_s_service_subscribe_a"
  "ck\020\206\240\001\022 \n\032id_s_service_subscribe_brd\020\206\300\001"
  "b\006proto3"
  ;
static ::_pbi::once_flag descriptor_table_msg_5fid_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_msg_5fid_2eproto = {
    false, false, 608, descriptor_table_protodef_msg_5fid_2eproto,
    "msg_id.proto",
    &descriptor_table_msg_5fid_2eproto_once, nullptr, 0, 0,
    schemas, file_default_instances, TableStruct_msg_5fid_2eproto::offsets,
    nullptr, file_level_enum_descriptors_msg_5fid_2eproto,
    file_level_service_descriptors_msg_5fid_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_msg_5fid_2eproto_getter() {
  return &descriptor_table_msg_5fid_2eproto;
}

// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_msg_5fid_2eproto(&descriptor_table_msg_5fid_2eproto);
namespace game {
const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor* message_type_descriptor() {
  ::PROTOBUF_NAMESPACE_ID::internal::AssignDescriptors(&descriptor_table_msg_5fid_2eproto);
  return file_level_enum_descriptors_msg_5fid_2eproto[0];
}
bool message_type_IsValid(int value) {
  switch (value) {
    case 0:
    case 4096:
    case 8192:
    case 12288:
    case 16384:
    case 20480:
    case 24576:
    case 61440:
      return true;
    default:
      return false;
  }
}

const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor* message_id_descriptor() {
  ::PROTOBUF_NAMESPACE_ID::internal::AssignDescriptors(&descriptor_table_msg_5fid_2eproto);
  return file_level_enum_descriptors_msg_5fid_2eproto[1];
}
bool message_id_IsValid(int value) {
  switch (value) {
    case 0:
    case 16385:
    case 16386:
    case 16387:
    case 16388:
    case 16390:
    case 20481:
    case 20482:
    case 20483:
    case 20484:
    case 20486:
    case 24578:
    case 24581:
    case 24582:
      return true;
    default:
      return false;
  }
}


// @@protoc_insertion_point(namespace_scope)
}  // namespace game
PROTOBUF_NAMESPACE_OPEN
PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)
#include <google/protobuf/port_undef.inc>
