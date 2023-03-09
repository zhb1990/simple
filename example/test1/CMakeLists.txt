cmake_minimum_required(VERSION 3.5)

# Set the project name
project(test1)

# test1
add_library(test1 SHARED "test1.cpp")
add_dependencies(test1 libruntime)
target_link_libraries(test1 PRIVATE libruntime)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
