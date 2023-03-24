#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <chrono>

#include "KVS/InMemoryKVS.hpp"
#include "node/Cluster.hpp"
#include "node/Cluster.cpp"
#include "net/Socket.hpp"
#include "node/ProtocolHandler.hpp"
#include "node/node.hpp"

using namespace std::chrono_literals; // NOLINT
using namespace node::cluster; // NOLINT
using namespace node; // NOLINT

void compare_clusterNodes(const ClusterNode& lhs, const ClusterNode& rhs) {
    CHECK_EQ(lhs.name, rhs.name);
    CHECK_EQ(lhs.ip, rhs.ip);
    CHECK_EQ(lhs.cluster_port, rhs.cluster_port);
    CHECK_EQ(lhs.client_port, rhs.client_port);
    CHECK_EQ(lhs.served_slots, rhs.served_slots);
    CHECK_EQ(lhs.num_slots_served, rhs.num_slots_served);

    for (int i = 0; i < CLUSTER_AMOUNT_OF_SLOTS; i++) {
        CHECK_EQ(lhs.served_slots[i], rhs.served_slots[i]);
    }
}

TEST_CASE("Test Gossip Ping") {

    ClusterState state_receiver{};
    ClusterState state_sender{};
    ClusterNode dummy{};
    state_receiver.slots.resize(3);
    state_sender.slots.resize(3);

    uint16_t cluster_port{ 3000 };
    ClusterNode node1{ "node1", "127.0.0.1", cluster_port, 1235 };
    ClusterNode sender{ "sender", "127.0.0.1", cluster_port, 1236 };

    state_sender.nodes["node1"] = node1;
    state_sender.size = 1;
    state_sender.myself = sender;
    state_receiver.size = 0;


    auto send_ping = [&]() {
        net::Socket sender_socket{};
        net::Connection connection = sender_socket.connect(cluster_port);
        node::cluster::send_ping(connection, state_sender);
    };

    auto handle_ping = [&]() {
        net::Socket receiver_socket{};
        if (!net::is_listening(cluster_port)) {
            receiver_socket.listen(cluster_port);
        }

        net::Connection connection = receiver_socket.accept();

        //Receive metadata, because that is not handled by the handle_ping function
        auto meta_data = node::protocol::get_metadata(connection);
        node::cluster::handle_ping(connection, state_receiver, meta_data.payload_size);
    };

    SUBCASE("Insert new") {
        CHECK_EQ(state_receiver.nodes.size(), 0);

        auto received = std::async(handle_ping);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_ping);

        sent.get();
        received.get();

        CHECK_EQ(state_receiver.nodes.size(), 2);
        compare_clusterNodes(state_receiver.nodes["node1"], node1);
        compare_clusterNodes(state_receiver.nodes["sender"], sender);
    }

    SUBCASE("Overwrite existing") {
        state_receiver.nodes["node1"] = node1;
        node1.num_slots_served = 100;
        node1.served_slots[0] = true;
        node1.num_slots_served = node1.served_slots.count();
        state_receiver.nodes["sender"] = sender;

        state_sender.nodes["node1"] = node1;
        CHECK_EQ(state_receiver.nodes.size(), 2);

        auto received = std::async(handle_ping);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_ping);

        sent.get();
        received.get();

        CHECK_EQ(state_receiver.nodes.size(), 2);
        compare_clusterNodes(state_receiver.nodes["node1"], node1);
        compare_clusterNodes(state_receiver.nodes["node1"], *state_receiver.slots[0].served_by);
        compare_clusterNodes(state_receiver.nodes["sender"], sender);
    }

}

TEST_CASE("Hashing") {
    auto expected = static_cast<uint16_t>(std::hash<std::string>{}("test"));
    auto actual = node::cluster::get_key_hash("test");
    CHECK_EQ(expected, actual);

    expected = static_cast<uint16_t>(std::hash<std::string>{}("test2"));
    actual = node::cluster::get_key_hash("test2");
    CHECK_EQ(expected, actual);

    expected = static_cast<uint16_t>(std::hash<std::string>{}("test"));
    actual = node::cluster::get_key_hash("{test}3");
    CHECK_EQ(expected, actual);

    expected = static_cast<uint16_t>(std::hash<std::string>{}("tes{t"));
    actual = node::cluster::get_key_hash("{tes{t}{3}}}");
    CHECK_EQ(expected, actual);

    expected = static_cast<uint16_t>(std::hash<std::string>{}("tes}t{3"));
    actual = node::cluster::get_key_hash("tes}t{3");
    CHECK_EQ(expected, actual);

    expected = static_cast<uint16_t>(std::hash<std::string>{}("3"));
    actual = node::cluster::get_key_hash("tes}t{3}");
    CHECK_EQ(expected, actual);
}

std::string get_key_with_target_slot(int slot) {
    std::string key = "key";
    while (node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS != slot) {
        key += "1";
    }
    return key;
}

TEST_CASE("test sharding") {
    key_value_store::InMemoryKVS kvs1{};
    key_value_store::InMemoryKVS kvs2{};

    ClusterNode node1{ "node1", "127.0.0.1", 3002, 3000, std::bitset<CLUSTER_AMOUNT_OF_SLOTS>{}, 0 };
    ClusterNode node2 = node1;
    std::vector<Slot> slots{ {&node1, 0, SlotState::c_NORMAL}, { &node1, 1, SlotState::c_NORMAL }, { &node2, 2, SlotState::c_NORMAL } };
    node2.client_port = 9000;

    node1.served_slots[0] = true;
    node1.served_slots[1] = true;
    node2.served_slots[2] = true;

    ClusterState state1{
        {{"node1", node1}, {"node2", node2}},
        2,
        slots,
        node1 };

    ClusterState state2{
        {{"node1", node1}, {"node2", node2}},
        2,
        slots,
        node2 };

    Node server1 = Node::new_in_memory_node(state1);
    Node server2 = Node::new_in_memory_node(state2);

    int port{ 3000 };

    auto process_instruction = [&](Node& server) {
        net::Socket socket{};
        if (!net::is_listening(socket.fd())) {
            socket.listen(port);
        }

        net::Connection connection = socket.accept();
        server.handle_connection(connection);
    };

    auto send_instruction = [&](Node& server, const protocol::command command, protocol::Instruction instruction, const std::string& payload) {
        net::Connection connection = net::Socket{}.connect(port);
        node::protocol::send_instruction(connection, command, instruction, payload);

        auto received_metadata = protocol::get_metadata(connection);
        auto received_command = protocol::get_command(connection, received_metadata.argc, received_metadata.command_size);
        ByteArray received_payload = protocol::get_payload(connection, received_metadata.payload_size);

        return std::make_tuple(received_metadata, received_command, received_payload);
    };

    SUBCASE("Test correct node") {
        std::string key = get_key_with_target_slot(0);

        auto received = std::async(process_instruction, std::ref(server1));
        std::this_thread::sleep_for(100ms);

        auto sent = std::async(send_instruction, std::ref(server2), protocol::command{ key, "10", "0" }, protocol::Instruction::c_PUT, "TestVal123");

        auto [actual_metadata, actual_command, actual_payload] = sent.get();
        received.get();

        //Check metadata
        CHECK_EQ(actual_metadata.argc, 0);
        CHECK_EQ(actual_metadata.command_size, 0);
        CHECK_EQ(actual_metadata.payload_size, 0);
        CHECK_EQ(actual_metadata.instruction, protocol::Instruction::c_OK_RESPONSE);

        //Check command
        CHECK_EQ(actual_command.size(), 0);

        //Check payload
        CHECK_EQ(actual_payload.size(), 0);
    }

    SUBCASE("Test wrong node") {
        std::string key = get_key_with_target_slot(2);

        auto received = std::async(process_instruction, std::ref(server1));
        std::this_thread::sleep_for(100ms);

        auto sent = std::async(send_instruction, std::ref(server2), protocol::command{ key, "10", "0" }, protocol::Instruction::c_PUT, "TestVal123");
        auto [actual_metadata, actual_command, actual_payload] = sent.get();

        received.get();

        //Check metadata
        CHECK_EQ(actual_metadata.argc, 2);
        CHECK_EQ(actual_metadata.command_size, 29);
        CHECK_EQ(actual_metadata.payload_size, 0);
        CHECK_EQ(actual_metadata.instruction, protocol::Instruction::c_MOVE);

        //Check command
        CHECK_EQ(actual_command.size(), 2);
        CHECK_EQ(actual_command[0], "127.0.0.1");
        CHECK_EQ(actual_command[1], "9000");

        //Check payload
        CHECK_EQ(actual_payload.size(), 0);
    }
}