cmake_minimum_required(VERSION 3.5)

# Set the project name
project(gate_master)

# gate_master
add_library(gate_master SHARED "gate_master.cpp")
add_dependencies(gate_master libkernel)
target_link_libraries(gate_master PRIVATE libkernel)

