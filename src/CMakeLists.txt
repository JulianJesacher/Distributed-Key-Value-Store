cmake_minimum_required(VERSION 3.10)

# ByteArray library
add_library(ByteArray_l
    utils/ByteArray.hpp
    utils/ByteArray.cpp
)
target_include_directories(ByteArray_l PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# KeyValueStore library
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

# Networking library
add_library(Networking_l
    net/FileDescriptor.hpp
    net/FileDescriptor.cpp
    net/Socket.hpp
    net/Socket.cpp
    net/Connection.hpp
    net/Connection.cpp
    net/Epoll.hpp
    net/Epoll.cpp
)
target_include_directories(Networking_l PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Node library
add_library(Node_l
    node/ProtocolHandler.hpp
    node/ProtocolHandler.cpp
    node/Cluster.hpp
    node/Cluster.cpp
    net/FileDescriptor.hpp
    net/FileDescriptor.cpp
    net/Socket.hpp
    net/Socket.cpp
    net/Connection.hpp
    net/Connection.cpp
    client/Client.hpp
    client/Client.cpp
    utils/ByteArray.hpp
    utils/ByteArray.cpp
    node/InstructionHandler.hpp
    node/InstructionHandler.cpp
    KVS/IKeyValueStore.hpp
    KVS/InMemoryKVS.hpp
    KVS/InMemoryKVS.cpp
    utils/Status.hpp
    utils/Status.cpp
    node/node.hpp
    node/node.cpp
    net/Epoll.hpp
    net/Epoll.cpp
)
target_include_directories(Node_l PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Client library
add_library(Client_l
    client/Client.hpp
    client/Client.cpp
    node/ProtocolHandler.hpp
    node/ProtocolHandler.cpp
    node/Cluster.hpp
    node/Cluster.cpp
    net/FileDescriptor.hpp
    net/FileDescriptor.cpp
    net/Socket.hpp
    net/Socket.cpp
    net/Connection.hpp
    net/Connection.cpp
    client/Client.hpp
    client/Client.cpp
    utils/ByteArray.hpp
    utils/ByteArray.cpp
    node/InstructionHandler.hpp
    node/InstructionHandler.cpp
    KVS/IKeyValueStore.hpp
    KVS/InMemoryKVS.hpp
    KVS/InMemoryKVS.cpp
    utils/Status.hpp
    utils/Status.cpp
    node/node.hpp
    node/node.cpp
    net/Epoll.hpp
    net/Epoll.cpp
)
target_include_directories(Client_l PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Node executable
add_executable(Node node/main.cpp)
target_link_libraries(Node Node_l)
target_link_libraries(Node pthread)

#Client-cli executable
add_executable(Client-cli client/Client-cli.cpp)
target_link_libraries(Client-cli Client_l)