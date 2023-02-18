cmake_minimum_required(VERSION 3.5)

# Set the project name
project(test)

# test
add_executable(test test_task.cpp test_network.cpp test_shm.cpp main.cpp)

enable_testing()

find_package(GTest CONFIG REQUIRED)
if (NOT GTest_FOUND)
    message(WARNING "GTest not found.")
endif ()

add_dependencies(test libruntime)
target_link_libraries(test PRIVATE libruntime GTest::gtest)

file(COPY ../common.vcxproj.user DESTINATION ${PROJECT_BINARY_DIR})
file(RENAME ${PROJECT_BINARY_DIR}/common.vcxproj.user ${PROJECT_BINARY_DIR}/test.vcxproj.user)

add_test(AllTestsInMain test)
