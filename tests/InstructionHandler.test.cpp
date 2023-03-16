#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <chrono>

#include "NetworkingHelper.hpp"
#include "node/InstructionHandler.hpp"
#include "net/Connection.hpp"
#include "KVS/InMemoryKVS.hpp"
#include "KVS/IKeyValueStore.hpp"
#include "net/Socket.hpp"

using namespace  std::chrono_literals;

std::pair<std::string, node::protocol::MetaData> get_command(node::protocol::Instruction i,
    const std::vector<std::string>& command, const std::string& payload = "") {

    uint16_t argc = command.size() + (payload.empty() ? 0 : 1);
    uint64_t command_size = std::accumulate(command.begin(), command.end(), 0,
        [](auto acc, auto s) { return acc + s.size() + 4; });
    node::protocol::MetaData m{argc, i, command_size, payload.size()};

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

    net::Socket socket{};
    int port{ 3000 };

    //PUT "key" payload_size:4 offset:0 "value"
    //Metadata: argc:3 instruction:PUT command_size:17 payload_size:4
    //Command: arg1: key arg1Len: 3 arg2: 5 arg2Len: 1 arg3: 0 arg3Len: 1
    std::string key{ "key" }, value{ "value" };
    node::protocol::command command{"key", "5", "0"};
    auto [command_data, m] = get_command(node::protocol::Instruction::c_PUT, command, value);

    auto send_command = [&](std::string v) {
        networkingHelper::send(port, v.c_str(), v.size());
    };

    auto process_command = [&](node::protocol::MetaData meta_data) {
        if (!net::is_listening(socket.fd())) {
            socket.listen(port);
        }
        net::Connection c = socket.accept();
        node::instruction_handler::handle_put(c, meta_data, command, kvs);
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
    command = node::protocol::command{ "key", "15", "5" };
    auto [command_data2, m2] = get_command(node::protocol::Instruction::c_PUT, command, value);
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