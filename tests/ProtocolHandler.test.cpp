#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <chrono>

#include "node/ProtocolHandler.hpp"
#include "NetworkingHelper.hpp"
#include "client/Client.hpp"
#include "net/Socket.hpp"
#include "net/Connection.hpp"

using namespace  std::chrono_literals;

TEST_CASE("Get Metadata") {
    node::protocol::MetaData meta_data{1, node::protocol::Instruction::c_GET, 3, 4};

    //Convert metadata to network byte order
    node::protocol::MetaData converted_meta_data = meta_data;
    converted_meta_data.argc = htons(meta_data.argc);
    converted_meta_data.command_size = htobe64(meta_data.command_size);
    converted_meta_data.payload_size = htobe64(meta_data.payload_size);


    net::Socket socket{};
    int port{ 3000 };

    auto send_metadata = [&]() {
        networkingHelper::send(port, reinterpret_cast<const char*>(&converted_meta_data), sizeof(converted_meta_data));
        return meta_data;
    };

    auto parse_metadata = [&]() {
        socket.listen(port);
        net::Connection c = socket.accept();
        return node::protocol::get_metadata(c);
    };

    auto received = std::async(parse_metadata);
    std::this_thread::sleep_for(100ms);
    auto sent = std::async(send_metadata);

    auto received_data = received.get();

    CHECK(memcmp(&meta_data, &received_data, sizeof(meta_data)) == 0);
    CHECK_EQ(meta_data.argc, received_data.argc);
    CHECK_EQ(meta_data.instruction, received_data.instruction);
    CHECK_EQ(meta_data.command_size, received_data.command_size);
    CHECK_EQ(meta_data.payload_size, received_data.payload_size);
}


TEST_CASE("Get Commands") {
    //Command: arg1 arg2 arg3
    uint64_t arg1_len{ 4 }, arg2_len{ 4 }, arg3_len{ 4 };
    std::string arg1{"arg1"}, arg2{ "arg2" }, arg3{ "arg3" };
    constexpr int command_size = 36; //arg1_len, arg1, arg2_len, arg2, arg3_len, arg3;
    std::array<char, command_size> command_data;

    //Convert size to network byte order
    uint64_t converted_arg1_len = htobe64(arg1_len);
    uint64_t converted_arg2_len = htobe64(arg2_len);
    uint64_t converted_arg3_len = htobe64(arg3_len);

    //Create buffer with command data
    int offset{ 0 };
    std::memcpy(command_data.data() + offset, &converted_arg1_len, sizeof(converted_arg1_len));
    offset += sizeof(converted_arg1_len);
    std::memcpy(command_data.data() + offset, arg1.data(), arg1_len);
    offset += arg1_len;
    std::memcpy(command_data.data() + offset, &converted_arg2_len, sizeof(converted_arg2_len));
    offset += sizeof(converted_arg2_len);
    std::memcpy(command_data.data() + offset, arg2.data(), arg2_len);
    offset += arg2_len;
    std::memcpy(command_data.data() + offset, &converted_arg3_len, sizeof(converted_arg3_len));
    offset += sizeof(converted_arg3_len);
    std::memcpy(command_data.data() + offset, arg3.data(), arg3_len);


    net::Socket socket{};
    int port{ 3000 };

    auto send_command = [&]() {
        networkingHelper::send(port, command_data.data(), command_size);
    };

    auto parse_command = [&]() {
        socket.listen(port);
        net::Connection c = socket.accept();
        return node::protocol::get_command(c, 3, command_size);
    };

    auto received = std::async(parse_command);
    std::this_thread::sleep_for(100ms);
    auto sent = std::async(send_command);

    auto received_data = received.get();
    CHECK_EQ(received_data.size(), 3);
    CHECK_EQ(received_data[0], arg1);
    CHECK_EQ(received_data[1], arg2);
    CHECK_EQ(received_data[2], arg3);
}

TEST_CASE("Get Payload") {
    std::string payload{"This is a test payload!"};

    net::Socket socket{};
    int port{ 3000 };

    auto send_command = [&]() {
        networkingHelper::send(port, payload.data(), payload.size());
    };

    auto parse_byte_array = [&]() {
        socket.listen(port);
        net::Connection c = socket.accept();
        return node::protocol::get_payload(c, payload.size());
    };

    auto received = std::async(parse_byte_array);
    std::this_thread::sleep_for(100ms);
    auto sent = std::async(send_command);

    auto received_data = received.get();
    auto expected_data = ByteArray::new_allocated_byte_array(payload);

    CHECK_EQ(received_data.size(), expected_data.size());
    CHECK(memcmp(received_data.data(), expected_data.data(), expected_data.size()) == 0);
}
