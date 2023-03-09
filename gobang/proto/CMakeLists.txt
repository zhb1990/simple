cmake_minimum_required(VERSION 3.5)

# Set the project name
project(libproto)

file(GLOB
        PROTO_HEADS
        proto.hpp
        msg_id.pb.h
        msg_ec.pb.h
        msg_base.pb.h
        msg_server.pb.h
        msg_client.pb.h
        )

file(GLOB
        PROTO_SOURCES
        msg_id.pb.cc
        msg_ec.pb.cc
        msg_base.pb.cc
        msg_server.pb.cc
        msg_client.pb.cc
        )

add_library(${PROJECT_NAME} SHARED ${PROTO_HEADS} ${PROTO_SOURCES})

if (WIN32)
    source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${PROTO_HEADS} ${PROTO_SOURCES})
    target_compile_definitions(${PROJECT_NAME} INTERFACE PROTO_DLL_IMPORT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PROTO_DLL_EXPORT)
else ()
    target_compile_definitions(${PROJECT_NAME} PUBLIC PROTO_LIB_VISIBILITY)
endif ()


target_include_directories(${PROJECT_NAME}
        PUBLIC ${PROJECT_SOURCE_DIR}
        )

find_package(Protobuf CONFIG REQUIRED)
if (NOT Protobuf_FOUND)
    message(WARNING "protobuf not found.")
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} PUBLIC protobuf::libprotobuf)
