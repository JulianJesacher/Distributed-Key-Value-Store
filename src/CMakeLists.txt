cmake_minimum_required(VERSION 3.10)

#ByteArray library
add_library(ByteArray_l
    utils/ByteArray.hpp
    utils/ByteArray.cpp
)
target_include_directories(ByteArray_l PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#KeyValueStore library
add_library(KeyValueStore_l 
    KVS/IKeyValueStore.hpp
    KVS/InMemoryKVS.hpp
    KVS/InMemoryKVS.cpp
    utils/ByteArray.hpp
    utils/ByteArray.cpp
    utils/Status.hpp
    utils/Status.cpp
)
target_include_directories(KeyValueStore_l PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#Networking library
add_library(Networking_l
    net/FileDescriptor.hpp
    net/FileDescriptor.cpp
)
target_include_directories(Networking_l PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})