cmake_minimum_required(VERSION 3.5)

# Set the project name
project(gate)

# gate
file(GLOB
        GATE_HEADS
        gate.h
        local_listener.h
        master_connector.h
        remote_connector.h
        remote_listener.h
        )

file(GLOB
        GATE_SOURCES
        gate.cpp
        local_listener.cpp
        master_connector.cpp
        remote_connector.cpp
        remote_listener.cpp
        )

add_library(gate SHARED ${GATE_HEADS} ${GATE_SOURCES})
add_dependencies(gate libkernel)
target_link_libraries(gate PRIVATE libkernel)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
