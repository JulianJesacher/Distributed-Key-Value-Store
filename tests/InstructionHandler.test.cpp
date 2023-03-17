#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <chrono>
#include <numeric>

#include "node/InstructionHandler.hpp"
#include "net/Connection.hpp"
#include "KVS/InMemoryKVS.hpp"
#include "KVS/IKeyValueStore.hpp"
#include "net/Socket.hpp"
#include "node/ProtocolHandler.hpp"

using namespace  std::chrono_literals;
using namespace node;

std::pair<std::string, protocol::MetaData> get_command(protocol::Instruction i,
    const std::vector<std::string>& command, const std::string& payload = "") {

    uint16_t argc = command.size() + (payload.empty() ? 0 : 1);
    uint64_t command_size = std::accumulate(command.begin(), command.end(), 0,
        [](auto acc, auto s) { return acc + s.size() + 4; });
    protocol::MetaData m{argc, i, command_size, payload.size()};

    std::string serialized_command{reinterpret_cast<const char*>(&m), sizeof(m)};
    for (auto& s : command) {
        uint64_t size = s.size();
        serialized_command += std::string{reinterpret_cast<const char*>(&size), sizeof(size)};
        serialized_command += s;
    }
    return { serialized_command, m };
}

TEST_CASE("Test put") {
    key_value_store::InMemoryKVS kvs{};
    ByteArray ba{};
    Status st = kvs.get("key", ba);
    CHECK_EQ(kvs.get_size(), 0);
    CHECK(st.is_not_found());

    int port{ 3000 };

    //PUT key:"key" payload_size:4 offset:0 "value"
    //Metadata: argc:3 instruction:PUT command_size:17 payload_size:4
    //Command: arg1: key arg1Len: 3 arg2: 5 arg2Len: 1 arg3: 0 arg3Len: 1
    std::string key{ "key" }, value{ "value" };
    protocol::command command{"key", "5", "0"};
    auto [command_data, m] = get_command(protocol::Instruction::c_PUT, command, value);

    auto send_command = [&](std::string v) {
        net::Socket client{};
        net::Connection c = client.connect(port);
        //send the payload
        c.send(v.c_str(), v.size());
    };

    auto process_command = [&](protocol::MetaData meta_data) {
        net::Socket server{};
        if (!net::is_listening(server.fd())) {
            server.listen(port);
        }
        net::Connection c = server.accept();
        instruction_handler::handle_put(c, meta_data, command, kvs);
    };

    auto processed = std::async(process_command, m);
    std::this_thread::sleep_for(100ms);
    auto sent = std::async(send_command, value);

    processed.get();
    sent.get();

    CHECK_EQ(kvs.get_size(), 1);
    kvs.get("key", ba);
    CHECK_EQ(value, ba.to_string());


    //Increase size of underlying buffer array test, with offset
    value = "valuevaluevalue";
    command = protocol::command{ "key", "15", "5" };
    auto [command_data2, m2] = get_command(protocol::Instruction::c_PUT, command, value);
    m2.payload_size = 20;

    processed = std::async(process_command, m2);
    std::this_thread::sleep_for(100ms);
    sent = std::async(send_command, value);

    processed.get();
    sent.get();

    CHECK_EQ(kvs.get_size(), 1);
    kvs.get("key", ba);
    CHECK_EQ("valuevaluevaluevalue", ba.to_string());
}


TEST_CASE("Test get") {
    key_value_store::InMemoryKVS kvs{};
    int port{ 3000 };

    //GET key:"key" size:"5" offset:"0"
    //Metadata: argc:3 instruction:GET command_size:17 payload_size:0
    //Command: arg1: key arg1Len: 3 arg2: 5 arg2Len: 1 arg3: 0 arg3Len: 1
    std::string key{ "key" };
    protocol::command sent_command{"key", "5", "0"};
    auto [command_data, m] = get_command(protocol::Instruction::c_GET, sent_command);

    auto send_command = [&]() {
        net::Socket client{};
        net::Connection c = client.connect(port);

        auto metadata = protocol::get_metadata(c);
        auto command = protocol::get_command(c, 2, metadata.command_size);
        ByteArray payload = protocol::get_payload(c, metadata.payload_size);

        return std::make_tuple(metadata, command, payload);
    };

    auto process_command = [&]() {
        net::Socket server{};
        if (!net::is_listening(server.fd())) {
            server.listen(port);
        }
        net::Connection c = server.accept();
        instruction_handler::handle_get(c, m, sent_command, kvs);
    };

    //TODO: Check for correct error when not found

    ByteArray value = ByteArray::new_allocated_byte_array("value");
    kvs.put(key, value);

    auto processed = std::async(process_command);
    std::this_thread::sleep_for(100ms);
    auto sent = std::async(send_command);

    processed.get();
    auto [actual_metadata, actual_command, actual_payload] = sent.get();
    std::string expected_payload{ "value" };

    //Check metadata
    CHECK_EQ(2, actual_metadata.argc);
    CHECK_EQ(protocol::Instruction::c_GET_RESPONSE, actual_metadata.instruction);
    CHECK_EQ(18, actual_metadata.command_size);
    CHECK_EQ(5, actual_metadata.payload_size);

    //Check command
    CHECK_EQ("5", actual_command[0]);
    CHECK_EQ("0", actual_command[1]);

    //Check payload
    CHECK_EQ(expected_payload, actual_payload.to_string());
}
