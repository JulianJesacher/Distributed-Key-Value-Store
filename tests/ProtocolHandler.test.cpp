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
    node::protocol::MetaData m{1, node::protocol::Instruction::c_GET, 3, 4};
    net::Socket socket{};
    int port{ 3000 };

    auto send_metadata = [&]() {
        networkingHelper::send(port, reinterpret_cast<const char*>(&m), sizeof(m));
        return m;
    };

    auto parse_metadata = [&]() {
        socket.listen(port);
        net::Connection c = socket.accept();
        return node::protocol::get_metadata(c);
    };

    auto received = std::async(parse_metadata);
    std::this_thread::sleep_for(100ms);
    auto sent = std::async(send_metadata);

    auto sent_data = sent.get();
    auto received_data = received.get();

    CHECK(memcmp(&sent_data, &received_data, sizeof(sent_data)) == 0);
    CHECK_EQ(m.argc, received_data.argc);
    CHECK_EQ(m.instruction, received_data.instruction);
    CHECK_EQ(m.command_size, received_data.command_size);
    CHECK_EQ(m.payload_size, received_data.payload_size);
}


TEST_CASE("Get Commands") {
    //Command: arg1 arg2 arg3
    uint64_t arg1_len{ 4 }, arg2_len{ 4 }, arg3_len{ 4 };
    std::string arg1{"arg1"}, arg2{ "arg2" }, arg3{ "arg3" };
    constexpr int command_size = 36; //arg1_len, arg1, arg2_len, arg2, arg3_len, arg3;
    std::array<char, command_size> command_data;

    //Create buffer with command data
    int offset{ 0 };
    std::memcpy(command_data.data() + offset, &arg1_len, sizeof(arg1_len));
    offset += sizeof(arg1_len);
    std::memcpy(command_data.data() + offset, arg1.data(), arg1_len);
    offset += arg1_len;
    std::memcpy(command_data.data() + offset, &arg2_len, sizeof(arg2_len));
    offset += sizeof(arg2_len);
    std::memcpy(command_data.data() + offset, arg2.data(), arg2_len);
    offset += arg2_len;
    std::memcpy(command_data.data() + offset, &arg3_len, sizeof(arg3_len));
    offset += sizeof(arg3_len);
    std::memcpy(command_data.data() + offset, arg3.data(), arg3_len);


    net::Socket socket{};
    int port{ 3000 };

    auto send_command = [&]() {
        networkingHelper::send(port, command_data.data(), command_size);
    };

    auto parse_command = [&]() {
        socket.listen(port);
        net::Connection c = socket.accept();
        return node::protocol::get_command(c, 3, command_size, false);
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