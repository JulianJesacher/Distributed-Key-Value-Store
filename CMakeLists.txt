cmake_minimum_required(VERSION 3.14)
project(distributed_key_value_store)
find_package(doctest REQUIRED)

set(CMAKE_BUILD_TYPE "Debug")
add_subdirectory(test)
