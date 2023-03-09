cmake_minimum_required(VERSION 3.5)

# Set the project name
project(center)

# gate_master
add_library(center SHARED "center.cpp")
add_dependencies(center libkernel)
target_link_libraries(center PRIVATE libkernel)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
