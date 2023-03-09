cmake_minimum_required(VERSION 3.5)

# Set the project name
project(test2)

# test1
add_library(test2 SHARED "test2.cpp")
add_dependencies(test2 libruntime)
target_link_libraries(test2 PRIVATE libruntime)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
