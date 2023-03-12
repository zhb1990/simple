cmake_minimum_required(VERSION 3.5)

# Set the project name
project(unit_test)

# test
add_executable(unit_test test_task.cpp test_network.cpp test_shm.cpp test_event.cpp main.cpp)

find_package(GTest CONFIG REQUIRED)
if (NOT GTest_FOUND)
    message(WARNING "GTest not found.")
endif ()

add_dependencies(unit_test libruntime)
target_link_libraries(unit_test PRIVATE libruntime GTest::gtest)

file(COPY ../common.vcxproj.user DESTINATION ${PROJECT_BINARY_DIR})
file(RENAME ${PROJECT_BINARY_DIR}/common.vcxproj.user ${PROJECT_BINARY_DIR}/unit_test.vcxproj.user)


