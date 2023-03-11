cmake_minimum_required(VERSION 3.10)

add_library(ByteArray_l include/ByteArray.h utils/ByteArray.cpp)

target_include_directories(ByteArray_l PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})