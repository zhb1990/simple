cmake_minimum_required(VERSION 3.5)

# Set the project name
project(proxy)

# proxy
add_library(proxy SHARED "proxy.cpp")
add_dependencies(proxy libkernel)
target_link_libraries(proxy PRIVATE libkernel)

