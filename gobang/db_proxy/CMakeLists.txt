cmake_minimum_required(VERSION 3.5)

# Set the project name
project(db_proxy)

# db_proxy
add_library(db_proxy SHARED "db_proxy.cpp")
add_dependencies(db_proxy libkernel)

find_package(bsoncxx CONFIG REQUIRED)
if (NOT bsoncxx_FOUND)
    message(WARNING "bsoncxx not found.")
endif ()

find_package(mongocxx CONFIG REQUIRED)
if (NOT mongocxx_FOUND)
    message(WARNING "mongocxx not found.")
endif ()

target_link_libraries(db_proxy PRIVATE libkernel mongo::mongocxx_shared)

