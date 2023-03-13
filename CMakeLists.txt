cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 20)
project(distributed_key_value_store)
find_package(doctest REQUIRED)

set(CMAKE_BUILD_TYPE "Debug")
add_subdirectory(src)
add_subdirectory(tests)
