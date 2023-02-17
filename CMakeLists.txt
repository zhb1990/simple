cmake_minimum_required(VERSION 3.5)

# Set the project name
project(game)

if (${CMAKE_SIZEOF_VOID_P} MATCHES "4")
    message(FATAL_ERROR "only support x64 toolchain")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIGURATION>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIGURATION>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIGURATION>)

# vs 解决方案中的目录
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(runtime)
SET_PROPERTY(TARGET libruntime PROPERTY FOLDER "runtime")

# application
add_executable(executor "executor/main.cpp")
add_dependencies(executor libruntime)
target_link_libraries(executor PRIVATE libruntime)
file(COPY common.vcxproj.user DESTINATION ${PROJECT_BINARY_DIR})
file(RENAME ${PROJECT_BINARY_DIR}/common.vcxproj.user ${PROJECT_BINARY_DIR}/executor.vcxproj.user)

# test
add_subdirectory(test)
SET_PROPERTY(TARGET test PROPERTY FOLDER "test")

# model

## kernel
add_subdirectory(model/kernel)
SET_PROPERTY(TARGET libkernel PROPERTY FOLDER "model")

## proto
add_subdirectory(model/proto)
SET_PROPERTY(TARGET libproto PROPERTY FOLDER "model")

# example

## test1
add_subdirectory(example/test1)
SET_PROPERTY(TARGET test1 PROPERTY FOLDER "gobang")

## test2
add_subdirectory(example/test2)
SET_PROPERTY(TARGET test2 PROPERTY FOLDER "gobang")

# gobang

## gate_master
add_subdirectory(gobang/gate_master)
SET_PROPERTY(TARGET gate_master PROPERTY FOLDER "gobang")

## gate
add_subdirectory(gobang/gate)
SET_PROPERTY(TARGET gate PROPERTY FOLDER "gobang")

## proxy
add_subdirectory(gobang/proxy)
SET_PROPERTY(TARGET proxy PROPERTY FOLDER "gobang")













