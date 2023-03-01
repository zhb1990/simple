cmake_minimum_required(VERSION 3.5)

# Set the project name
project(libkernel)

file(GLOB
        KERNEL_HEADS
        headers.hpp
        proto_rpc.h
        kernel.hpp
        proto_utils.h
        gate_connector.h
        )

file(GLOB
        KERNEL_SOURCES
        proto_rpc.cpp
        proto_utils.cpp
        gate_connector.cpp
        )

add_library(${PROJECT_NAME} SHARED ${KERNEL_HEADS} ${KERNEL_SOURCES})

if (WIN32)
    source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${KERNEL_HEADS} ${KERNEL_SOURCES})
    target_compile_definitions(${PROJECT_NAME} INTERFACE KERNEL_DLL_IMPORT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE KERNEL_DLL_EXPORT)
else ()
    target_compile_definitions(${PROJECT_NAME} PUBLIC KERNEL_LIB_VISIBILITY)
endif ()


target_include_directories(${PROJECT_NAME}
        PUBLIC ${PROJECT_SOURCE_DIR}
        )

add_dependencies(${PROJECT_NAME} libruntime libproto)
target_link_libraries(${PROJECT_NAME} PUBLIC libruntime libproto)
