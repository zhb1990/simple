cmake_minimum_required(VERSION 3.5)

# Set the project name
project(gate)

# gate
file(GLOB
        GATE_HEADS
        gate.h
        service_info.h
        master_connector.h
        local_service.h
        local_listener.h
        remote_listener.h
        remote_gate.h
        )

file(GLOB
        GATE_SOURCES
        gate.cpp
        service_info.cpp
        master_connector.cpp
        local_service.cpp
        local_listener.cpp
        remote_listener.cpp
        remote_gate.cpp
        )

add_library(gate SHARED ${GATE_HEADS} ${GATE_SOURCES})
add_dependencies(gate libkernel)
target_link_libraries(gate PRIVATE libkernel)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
