#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <chrono>
#include <numeric>
#include <thread>

#include "node/InstructionHandler.hpp"
#include "net/Connection.hpp"
#include "KVS/InMemoryKVS.hpp"
#include "KVS/IKeyValueStore.hpp"
#include "net/Socket.hpp"
#include "node/ProtocolHandler.hpp"
#include "node/Cluster.hpp"

using namespace  std::chrono_literals; // NOLINT
using namespace node; // NOLINT

std::pair<std::string, protocol::MetaData> get_command_and_metadata(protocol::Instruction i,
    const std::vector<std::string>& command, const std::string& payload = "") {

    uint16_t argc = command.size() + (payload.empty() ? 0 : 1);
    uint64_t command_size = std::accumulate(command.begin(), command.end(), 0,
        [](auto acc, auto s) { return acc + s.size() + 4; });
    protocol::MetaData m{argc, i, command_size, payload.size()};

    std::string serialized_command{reinterpret_cast<const char*>(&m), sizeof(m)};
    for (const auto& s : command) {
        uint64_t size = s.size();
        serialized_command += std::string{reinterpret_cast<const char*>(&size), sizeof(size)};
        serialized_command += s;
    }
    return { serialized_command, m };
}

TEST_CASE("Test put") {
    //Test return value
    key_value_store::InMemoryKVS kvs{};
    ByteArray ba{};
    Status st = kvs.get("key", ba);
    cluster::ClusterState cluster_state{};
    cluster_state.myself = cluster::ClusterNode{};
    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; ++i) {
        cluster_state.myself.served_slots[i] = true;
    }

    CHECK_EQ(kvs.get_size(), 0);
    CHECK(st.is_not_found());
    int port{ 3000 };

    auto send_command = [&](std::string v) {
        net::Socket client{};
        net::Connection c = client.connect(port);
        c.send(v.c_str(), v.size());

        auto metadata = protocol::get_metadata(c);
        auto command = protocol::get_command(c, 0, metadata.command_size);
        ByteArray payload = protocol::get_payload(c, metadata.payload_size);

        return std::make_tuple(metadata, command, payload);
    };

    auto process_command = [&](const protocol::command& command, protocol::MetaData meta_data) {
        net::Socket server{};
        if (!net::is_listening(server.fd())) {
            server.listen(port);
        }
        net::Connection c = server.accept();
        instruction_handler::handle_put(c, meta_data, command, kvs, cluster_state);
    };

    SUBCASE("Insert first time") {
        //PUT key:"key" payload_size:4 offset:0 "value"
        //Metadata: argc:3 instruction:PUT command_size:17 payload_size:4
        //Command: arg1: key arg1Len: 3 arg2: 5 arg2Len: 1 arg3: 0 arg3Len: 1

        std::string key{ "key" };
        std::string value{ "value" };
        protocol::command command{"key", "5", "0"};
        auto [_, meta_data] = get_command_and_metadata(protocol::Instruction::c_PUT, command, value);
        auto processed = std::async(process_command, command, meta_data);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_command, value);

        processed.get();
        auto [actual_metadata, actual_command, actual_payload] = sent.get();

        //Check kvs state
        CHECK_EQ(kvs.get_size(), 1);
        kvs.get("key", ba);
        CHECK_EQ(value, ba.to_string());

        //Check response:
        //Check metadata
        CHECK_EQ(0, actual_metadata.argc);
        CHECK_EQ(protocol::Instruction::c_OK_RESPONSE, actual_metadata.instruction);
        CHECK_EQ(0, actual_metadata.command_size);
        CHECK_EQ(0, actual_metadata.payload_size);

        //Check command
        CHECK_EQ(0, actual_command.size());

        //Check payload
        CHECK_EQ(0, actual_payload.size());
    }


    SUBCASE("Increase size of underlying buffer with offset") {
        //PUT key:"key" payload_size:15 offset:5 "valuevaluevalue" // NOLINT
        //Metadata: argc:3 instruction:PUT command_size:18 payload_size:20
        //Command: arg1: key arg1Len: 3 arg2: 15 arg2Len: 2 arg3: 5 arg3Len: 1

        //Prepare kvs
        std::string key{ "key" }, value{ "valuevaluevalue" }; // NOLINT
        ByteArray ba = ByteArray::new_allocated_byte_array("value");
        kvs.put(key, ba);

        //Increase size of underlying buffer array test, with offset
        protocol::command command{ "key", "15", "5" };
        auto [_, meta_data] = get_command_and_metadata(protocol::Instruction::c_PUT, command, value);
        meta_data.payload_size = 20;
        auto processed = std::async(process_command, command, meta_data);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_command, value);

        processed.get();
        auto [actual_metadata, actual_command, actual_payload] = sent.get();

        //Check kvs state
        CHECK_EQ(kvs.get_size(), 1);
        kvs.get("key", ba);
        CHECK_EQ("valuevaluevaluevalue", ba.to_string()); // NOLINT

        //Check response:
        //Check metadata
        CHECK_EQ(0, actual_metadata.argc);
        CHECK_EQ(protocol::Instruction::c_OK_RESPONSE, actual_metadata.instruction);
        CHECK_EQ(0, actual_metadata.command_size);
        CHECK_EQ(0, actual_metadata.payload_size);

        //Check command
        CHECK_EQ(0, actual_command.size());

        //Check payload
        CHECK_EQ(0, actual_payload.size());
    }
}


TEST_CASE("Test get") {
    key_value_store::InMemoryKVS kvs{};
    cluster::ClusterState cluster_state{};
    cluster_state.myself = cluster::ClusterNode{};
    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; ++i) {
        cluster_state.myself.served_slots[i] = true;
    }

    int port{ 3000 };
    std::string key{ "key" };
    protocol::command sent_command{"key", "5", "0"};

    auto send_command = [&]() {
        net::Socket client{};
        net::Connection c = client.connect(port);

        auto metadata = protocol::get_metadata(c);
        auto command = protocol::get_command(c, 2, metadata.command_size);
        ByteArray payload = protocol::get_payload(c, metadata.payload_size);

        return std::make_tuple(metadata, command, payload);
    };

    auto process_command = [&](protocol::command command) {
        net::Socket server{};
        if (!net::is_listening(server.fd())) {
            server.listen(port);
        }
        net::Connection c = server.accept();
        instruction_handler::handle_get(c, command, kvs, cluster_state);
    };


    SUBCASE("Check for error when not found") {
        //GET key:"key" size:"5" offset:"0"
        //Metadata: argc:3 instruction:GET command_size:17 payload_size:0
        //Command: arg1: key arg1Len: 3 arg2: 5 arg2Len: 1 arg3: 0 arg3Len: 1

        auto processed = std::async(process_command, sent_command);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_command);
        processed.get();
        auto [actual_metadata, actual_command, actual_payload] = sent.get();

        //Check metadata
        CHECK_EQ(0, actual_metadata.argc);
        CHECK_EQ(protocol::Instruction::c_ERROR_RESPONSE, actual_metadata.instruction);
        CHECK_EQ(0, actual_metadata.command_size);
        CHECK_EQ(27, actual_metadata.payload_size);

        //Check command
        CHECK_EQ(0, actual_command.size());

        //Check payload
        CHECK_EQ(27, actual_payload.size());
        CHECK_EQ("The given key was not found", actual_payload.to_string());
    }


    SUBCASE("Check for correct response when found") {
        //GET key:"key" size:"5" offset:"0"
        //Metadata: argc:3 instruction:GET command_size:17 payload_size:0
        //Command: arg1: key arg1Len: 3 arg2: 5 arg2Len: 1 arg3: 0 arg3Len: 1

        //Prepare kvs
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        kvs.put(key, value);

        auto processed = std::async(process_command, sent_command);
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
        CHECK_EQ(2, actual_command.size());
        CHECK_EQ("5", actual_command[0]);
        CHECK_EQ("0", actual_command[1]);

        //Check payload
        CHECK_EQ(expected_payload, actual_payload.to_string());
    }
}


TEST_CASE("Test erase") {
    key_value_store::InMemoryKVS kvs{};
    cluster::ClusterState cluster_state{};
    cluster_state.myself = cluster::ClusterNode{};
    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; ++i) {
        cluster_state.myself.served_slots[i] = true;
    }

    int port{ 3000 };
    std::string key{ "key" };
    protocol::command sent_command{"key"};

    auto send_command = [&]() {
        net::Socket client{};
        net::Connection c = client.connect(port);

        auto metadata = protocol::get_metadata(c);
        auto command = protocol::get_command(c, 0, metadata.command_size);
        ByteArray payload = protocol::get_payload(c, metadata.payload_size);

        return std::make_tuple(metadata, command, payload);
    };

    auto process_command = [&](protocol::command command) {
        net::Socket server{};
        if (!net::is_listening(server.fd())) {
            server.listen(port);
        }

        net::Connection c = server.accept();
        instruction_handler::handle_erase(c, command, kvs, cluster_state);
    };

    SUBCASE("Check for error when not found") {
        //ERASE key:"key"
        //Metadata: argc:1 instruction:ERASE command_size:0 payload_size:0
        //command: arg1: key arg1Len: 3

        auto processed = std::async(process_command, sent_command);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_command);
        processed.get();
        auto [actual_metadata, actual_command, actual_payload] = sent.get();

        //Check metadata
        CHECK_EQ(0, actual_metadata.argc);
        CHECK_EQ(protocol::Instruction::c_ERROR_RESPONSE, actual_metadata.instruction);
        CHECK_EQ(0, actual_metadata.command_size);
        CHECK_EQ(27, actual_metadata.payload_size);

        //Check command
        CHECK_EQ(0, actual_command.size());

        //Check payload
        CHECK_EQ(27, actual_payload.size());
        CHECK_EQ("The given key was not found", actual_payload.to_string());
    }

    SUBCASE("Check for correct response when found") {
        //ERASE key:"key"
        //Metadata: argc:1 instruction:ERASE command_size:0 payload_size:0
        //command: arg1: key arg1Len: 3

        //prepare kvs
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        kvs.put(key, value);

        auto processed = std::async(process_command, sent_command);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_command);
        processed.get();
        auto [actual_metadata, actual_command, actual_payload] = sent.get();

        CHECK_EQ(0, kvs.get_size());

        //Check metadata
        CHECK_EQ(0, actual_metadata.argc);
        CHECK_EQ(protocol::Instruction::c_OK_RESPONSE, actual_metadata.instruction);
        CHECK_EQ(0, actual_metadata.command_size);
        CHECK_EQ(0, actual_metadata.payload_size);

        //Check command
        CHECK_EQ(0, actual_command.size());

        //Check payload
        CHECK_EQ(0, actual_payload.size());
    }
}

TEST_CASE("Test meet") {
    int receiver_cluster_port{ 3000 };
    int new_node_client_port{ 3001 }, new_node_cluster_port{ 3002 };
    cluster::ClusterState cluster_state{};
    std::string new_node_ip{"127.0.0.1"}, new_node_name{ "new_node" };


    auto send_meet = [&]() {
        net::Socket client{};
        net::Connection c = client.connect(receiver_cluster_port);

        auto metadata = protocol::get_metadata(c);
        auto command = protocol::get_command(c, 0, metadata.command_size);
        ByteArray payload = protocol::get_payload(c, metadata.payload_size);

        return std::make_tuple(metadata, command, payload);
    };

    auto process_meet = [&](protocol::command command) {
        net::Socket server{};
        if (!net::is_listening(server.fd())) {
            server.listen(receiver_cluster_port);
        }

        net::Connection c = server.accept();
        instruction_handler::handle_meet(c, command, cluster_state);
    };

    auto node_listener = [&](int port) {
        net::Socket server{};
        if (!net::is_listening(server.fd())) {
            server.listen(port);
        }
        server.accept();
        return true;
    };

    protocol::command sent_command{
        new_node_ip,
            std::to_string(new_node_client_port),
            std::to_string(new_node_cluster_port),
            new_node_name};

    auto listener = std::async(node_listener, new_node_cluster_port);
    std::this_thread::sleep_for(100ms);

    auto processed = std::async(process_meet, sent_command);
    std::this_thread::sleep_for(100ms);

    auto sent = std::async(send_meet);

    processed.get();
    auto [actual_metadata, actual_command, actual_payload] = sent.get();
    auto connected = listener.get();

    CHECK_EQ(1, cluster_state.size);
    CHECK(connected);

    //Check node
    cluster::ClusterNode& node = cluster_state.nodes[new_node_name];
    CHECK_EQ(new_node_name, std::string(node.name.begin()));
    CHECK_EQ(new_node_ip, std::string(node.ip.begin()));
    CHECK_EQ(new_node_client_port, node.client_port);
    CHECK_EQ(new_node_cluster_port, node.cluster_port);

    //Check metadata
    CHECK_EQ(0, actual_metadata.argc);
    CHECK_EQ(protocol::Instruction::c_OK_RESPONSE, actual_metadata.instruction);
    CHECK_EQ(0, actual_metadata.command_size);
    CHECK_EQ(0, actual_metadata.payload_size);

    //Check command
    CHECK_EQ(0, actual_command.size());

    //Check payload
    CHECK_EQ(0, actual_payload.size());

    std::cout << actual_payload.to_string() << std::endl;
}