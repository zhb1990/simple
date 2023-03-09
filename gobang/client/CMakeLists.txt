cmake_minimum_required(VERSION 3.5)

# Set the project name
project(client)

# client
add_library(client SHARED "client.cpp")
add_dependencies(client libkernel)
target_link_libraries(client PRIVATE libkernel)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
