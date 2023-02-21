cmake_minimum_required(VERSION 3.5)

# Set the project name
project(login)

# gate_master
add_library(login SHARED "login.cpp")
add_dependencies(login libkernel)
target_link_libraries(login PRIVATE libkernel)

