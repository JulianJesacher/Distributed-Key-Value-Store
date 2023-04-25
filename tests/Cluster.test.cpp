#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <chrono>
#include <sys/epoll.h>
#include <poll.h>
#include <thread>

#include "KVS/InMemoryKVS.hpp"
#include "node/Cluster.hpp"
#include "node/Cluster.cpp"
#include "net/Socket.hpp"
#include "node/ProtocolHandler.hpp"
#include "node/node.hpp"
#include "net/Epoll.hpp"

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

void compare_slots(const std::vector<Slot>& lhs, const std::vector<Slot>& rhs) {
    CHECK_EQ(lhs.size(), rhs.size());
    for (int i = 0; i < lhs.size(); i++) {
        CHECK_EQ(lhs[i].amount_of_keys, rhs[i].amount_of_keys);
        CHECK_EQ(lhs[i].state, rhs[i].state);

        //Compare migration partner
        if (lhs[i].migration_partner == nullptr || rhs[i].migration_partner == nullptr) {
            CHECK_EQ(lhs[i].migration_partner, rhs[i].migration_partner);
        }
        else {
            ClusterNode lhs_migration_partner = *lhs[i].migration_partner;
            ClusterNode rhs_migration_partner = *rhs[i].migration_partner;
            compare_clusterNodes(lhs_migration_partner, rhs_migration_partner);
        }

        //Compare served by
        if (lhs[i].served_by == nullptr || rhs[i].served_by == nullptr) {
            CHECK_EQ(lhs[i].served_by, rhs[i].served_by);
        }
        else {
            ClusterNode lhs_served_by = *lhs[i].served_by;
            ClusterNode rhs_served_by = *rhs[i].served_by;
            compare_clusterNodes(lhs_served_by, rhs_served_by);
        }
    }
}

std::string get_key_with_target_slot(int slot, std::vector<std::string> distinct = {}) {
    std::string key = "key";
    while (node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS != slot
        || std::find(distinct.begin(), distinct.end(), key) != distinct.end()) {
        key += "1";
    }
    return key;
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

    // set up slots
    state_sender.slots.resize(3);
    state_sender.slots[0].served_by = &state_sender.nodes["node1"];
    state_sender.slots[0].amount_of_keys = 10;
    state_sender.slots[0].state = SlotState::c_NORMAL;
    state_sender.slots[1].served_by = &state_sender.myself;
    state_sender.slots[1].amount_of_keys = 10;
    state_sender.slots[1].state = SlotState::c_IMPORTING;
    state_sender.slots[2].served_by = nullptr;
    state_sender.slots[2].amount_of_keys = 0;
    state_sender.slots[2].state = SlotState::c_MIGRATING;
    state_sender.slots[2].migration_partner = &state_sender.myself;

    auto send_ping = [&]() {
        net::Socket sender_socket{};
        net::Connection connection = sender_socket.connect(cluster_port);
        node::cluster::send_ping(&connection, state_sender);
    };

    auto handle_ping = [&]() {
        net::Socket receiver_socket{};
        if (!net::is_listening(cluster_port)) {
            receiver_socket.listen(cluster_port);
        }

        net::Connection connection = receiver_socket.accept();

        //Receive metadata, because that is not handled by the handle_ping function
        auto meta_data = node::protocol::get_metadata(connection);
        auto command = node::protocol::get_command(connection, meta_data.argc, meta_data.command_size);
        node::cluster::handle_ping(connection, state_receiver, command);
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
        compare_slots(state_receiver.slots, state_sender.slots);
    }
    /*
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
        */
}

/*
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


TEST_CASE("test sharding") {
    uint16_t client_port1{ 3000 }, client_port2{ 9000 }, cluster_port{ 3001 };
    Node server1 = Node::new_in_memory_node("node1", client_port1, cluster_port, "127.0.0.1");
    Node server2 = Node::new_in_memory_node("node2", client_port2, cluster_port, "127.0.0.1");

    ClusterNode node1{ "node1", "127.0.0.1", cluster_port, client_port1, std::bitset<CLUSTER_AMOUNT_OF_SLOTS>{}, 0 };
    ClusterNode node2 = node1;
    node2.client_port = client_port2;
    std::vector<Slot> slots{ {&node1, 0, SlotState::c_NORMAL}, { &node1, 1, SlotState::c_NORMAL }, { &node2, 2, SlotState::c_NORMAL } };

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

    server1.set_cluster_state(state1);
    server2.set_cluster_state(state2);

    auto process_instruction = [&](Node& server, int port) {
        net::Socket socket{};
        if (!net::is_listening(socket.fd())) {
            socket.listen(port);
        }

        net::Connection connection = socket.accept();
        server.handle_connection(connection);
    };

    auto send_instruction = [&](const protocol::Command command, protocol::Instruction instruction, const std::string& payload, int port) {
        net::Socket socket{};
        net::Connection connection = socket.connect(port);
        node::protocol::send_instruction(connection, command, instruction, payload);

        auto received_metadata = protocol::get_metadata(connection);
        auto received_command = protocol::get_command(connection, received_metadata.argc, received_metadata.command_size);
        ByteArray received_payload = protocol::get_payload(connection, received_metadata.payload_size);

        return std::make_tuple(received_metadata, received_command, received_payload);
    };

    SUBCASE("Test correct node") {
        std::string key = get_key_with_target_slot(0);
        auto received = std::async(process_instruction, std::ref(server1), client_port1);
        std::this_thread::sleep_for(100ms);

        auto sent = std::async(send_instruction, protocol::Command{ key, "10", "0" }, protocol::Instruction::c_PUT, "TestVal123", client_port1);
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
        auto received = std::async(process_instruction, std::ref(server1), client_port1);
        std::this_thread::sleep_for(100ms);

        auto sent = std::async(send_instruction, protocol::Command{ key, "10", "0" }, protocol::Instruction::c_PUT, "TestVal123", client_port1);
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
        CHECK_EQ(actual_command[1], std::to_string(client_port2));

        //Check payload
        CHECK_EQ(actual_payload.size(), 0);
    }
}


TEST_CASE("Test migrating slot") {
    uint16_t cluster_port = 4000;
    uint16_t client_port0 = 3000, client_port1 = 3001;

    Node server0 = Node::new_in_memory_node("node0", client_port0, cluster_port, "127.0.0.1");
    Node server1 = Node::new_in_memory_node("node1", client_port1, cluster_port, "127.0.0.1");

    ClusterNode node0{ "node0", "127.0.0.1", cluster_port, client_port0, std::bitset<CLUSTER_AMOUNT_OF_SLOTS>{}, 0 };
    ClusterNode node1{ "node1", "127.0.0.1", cluster_port, client_port1, std::bitset<CLUSTER_AMOUNT_OF_SLOTS>{}, 0 };

    //Initially, node1 serves slot0 and node2 serves slot1
    node0.served_slots[0] = true;
    node0.num_slots_served = 1;

    node1.served_slots[1] = true;
    node1.num_slots_served = 1;

    std::unordered_map<std::string, ClusterNode> nodes{ {"node0", node0}, { "node1", node1 } };
    std::vector<Slot> slots{ {&node0, 0, SlotState::c_NORMAL}, { &node1, 1, SlotState::c_NORMAL } };

    ClusterState state0{
        nodes,
        2,
        slots,
        node0 };
    ClusterState state1{
        nodes,
        2,
        slots,
        node1 };

    server0.set_cluster_state(state0);
    server1.set_cluster_state(state1);

    //Set up connection from node0 to node1
    net::Epoll epoll{};
    net::Socket socket0{}, socket1{};
    socket1.listen(cluster_port);
    auto connection0 = socket0.connect(cluster_port);
    auto connection1 = socket1.accept();

    epoll.add_event(connection1.fd());
    epoll.reset_occurred_events();
    server0.get_cluster_state().nodes["node1"].outgoing_link = connection0;


    auto process_instruction = [&](Node& server, uint16_t port) {
        net::Socket socket{};
        if (!net::is_listening(socket.fd())) {
            socket.listen(port);
        }

        net::Connection connection = socket.accept();
        server.handle_connection(connection);
    };

    auto send_instruction = [&](uint16_t port, const protocol::Command command, protocol::Instruction instruction, const std::string& payload) {
        net::Connection connection = net::Socket{}.connect(port);
        node::protocol::send_instruction(connection, command, instruction, payload);

        auto received_metadata = protocol::get_metadata(connection);
        auto received_command = protocol::get_command(connection, received_metadata.argc, received_metadata.command_size);
        ByteArray received_payload = protocol::get_payload(connection, received_metadata.payload_size);

        return std::make_tuple(received_metadata, received_command, received_payload);
    };


    SUBCASE("Test migrate slot1 from node0 to node1") {
        std::string key_slot_0 = get_key_with_target_slot(0);

        // Insert Key to slot 0 in node0
        auto processed = std::async(process_instruction, std::ref(server0), client_port0);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_instruction, client_port0, protocol::Command{ key_slot_0, "10", "0" }, protocol::Instruction::c_PUT, "TestVal123");
        sent.get();
        processed.get();


        //Check slot state of node0
        CHECK_EQ(1, server0.get_kvs().get_size());
        CHECK_EQ(1, server0.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(SlotState::c_NORMAL, server0.get_cluster_state().slots[0].state);




        // Set slot0 to migrating in node0
        processed = std::async(process_instruction, std::ref(server0), client_port0);
        std::this_thread::sleep_for(100ms);
        sent = std::async(send_instruction, client_port0, protocol::Command{ "0", "127.0.0.1", std::to_string(client_port1) }, protocol::Instruction::c_MIGRATE_SLOT, "");
        sent.get();
        processed.get();

        // Set slot0 to importing in node1
        processed = std::async(process_instruction, std::ref(server1), client_port1);
        std::this_thread::sleep_for(100ms);
        sent = std::async(send_instruction, client_port1, protocol::Command{ "0", "127.0.0.1", std::to_string(client_port0)}, protocol::Instruction::c_IMPORT_SLOT, "");
        sent.get();
        processed.get();

        // Check slot states
        CHECK_EQ(1, server0.get_kvs().get_size());
        CHECK_EQ(1, server0.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(SlotState::c_MIGRATING, server0.get_cluster_state().slots[0].state);
        CHECK_EQ(SlotState::c_IMPORTING, server1.get_cluster_state().slots[0].state);




        // Insert key with slot 0 in node0, expect ask response
        std::string other_key_slot_0 = get_key_with_target_slot(0, { key_slot_0 });

        processed = std::async(process_instruction, std::ref(server0), client_port0);
        std::this_thread::sleep_for(100ms);
        sent = std::async(send_instruction, client_port0, protocol::Command{ other_key_slot_0, "10", "0" }, protocol::Instruction::c_PUT, "TestVal123");
        auto [actual_metadata, actual_command, actual_payload] = sent.get();
        processed.get();

        CHECK_EQ(1, server0.get_kvs().get_size());
        CHECK_EQ(1, server0.get_cluster_state().slots[0].amount_of_keys);

        //Check metadata
        CHECK_EQ(actual_metadata.argc, 2);
        CHECK_EQ(actual_metadata.command_size, 8 + 9 + 8 + 4);
        CHECK_EQ(actual_metadata.payload_size, 0);
        CHECK_EQ(actual_metadata.instruction, protocol::Instruction::c_ASK);

        //Check command
        CHECK_EQ(actual_command.size(), 2);
        CHECK_EQ(actual_command[0], "127.0.0.1");
        CHECK_EQ(actual_command[1], std::to_string(client_port1));

        //Check payload
        CHECK_EQ(actual_payload.size(), 0);




        // Insert key with slot 0 in node1, expect ok response
        processed = std::async(process_instruction, std::ref(server1), client_port1);
        std::this_thread::sleep_for(100ms);
        sent = std::async(send_instruction, client_port1, protocol::Command{ other_key_slot_0, "10", "0" }, protocol::Instruction::c_PUT, "TestVal123");
        std::tie(actual_metadata, actual_command, actual_payload) = sent.get();
        processed.get();

        CHECK_EQ(1, server0.get_kvs().get_size());
        CHECK_EQ(1, server0.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(1, server1.get_kvs().get_size());
        CHECK_EQ(1, server1.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(SlotState::c_MIGRATING, server0.get_cluster_state().slots[0].state);
        CHECK_EQ(SlotState::c_IMPORTING, server1.get_cluster_state().slots[0].state);

        //Check metadata
        CHECK_EQ(actual_metadata.argc, 0);
        CHECK_EQ(actual_metadata.command_size, 0);
        CHECK_EQ(actual_metadata.payload_size, 0);
        CHECK_EQ(actual_metadata.instruction, protocol::Instruction::c_OK_RESPONSE);

        //Check command
        CHECK_EQ(actual_command.size(), 0);

        //Check payload
        CHECK_EQ(actual_payload.size(), 0);



        // Get key from node 0, expect ask response
        processed = std::async(process_instruction, std::ref(server0), client_port0);
        std::this_thread::sleep_for(100ms);
        sent = std::async(send_instruction, client_port0, protocol::Command{ other_key_slot_0, "10", "0", "false" }, protocol::Instruction::c_GET, "");
        std::tie(actual_metadata, actual_command, actual_payload) = sent.get();
        processed.get();

        CHECK_EQ(1, server0.get_kvs().get_size());
        CHECK_EQ(1, server0.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(1, server1.get_kvs().get_size());
        CHECK_EQ(1, server1.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(SlotState::c_MIGRATING, server0.get_cluster_state().slots[0].state);
        CHECK_EQ(SlotState::c_IMPORTING, server1.get_cluster_state().slots[0].state);

        //Check metadata
        CHECK_EQ(actual_metadata.argc, 2);
        CHECK_EQ(actual_metadata.command_size, 8 + 9 + 8 + 4);
        CHECK_EQ(actual_metadata.payload_size, 0);
        CHECK_EQ(actual_metadata.instruction, protocol::Instruction::c_ASK);

        //Check command
        CHECK_EQ(actual_command.size(), 2);
        CHECK_EQ(actual_command[0], "127.0.0.1");
        CHECK_EQ(actual_command[1], std::to_string(client_port1));

        //Check payload
        CHECK_EQ(actual_payload.size(), 0);



        //Get key from node1, expect no asking error
        processed = std::async(process_instruction, std::ref(server1), client_port1);
        std::this_thread::sleep_for(100ms);
        sent = std::async(send_instruction, client_port1, protocol::Command{ key_slot_0, "10", "0", "false" }, protocol::Instruction::c_GET, "");
        std::tie(actual_metadata, actual_command, actual_payload) = sent.get();
        processed.get();

        CHECK_EQ(1, server0.get_kvs().get_size());
        CHECK_EQ(1, server0.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(1, server1.get_kvs().get_size());
        CHECK_EQ(1, server1.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(SlotState::c_MIGRATING, server0.get_cluster_state().slots[0].state);
        CHECK_EQ(SlotState::c_IMPORTING, server1.get_cluster_state().slots[0].state);

        //Check metadata
        CHECK_EQ(actual_metadata.argc, 2);
        CHECK_EQ(actual_metadata.command_size, 8 + 9 + 8 + 4);
        CHECK_EQ(actual_metadata.payload_size, 0);
        CHECK_EQ(actual_metadata.instruction, protocol::Instruction::c_NO_ASKING_ERROR);

        //Check command
        CHECK_EQ(actual_command.size(), 2);
        CHECK_EQ(actual_command[0], "127.0.0.1");
        CHECK_EQ(actual_command[1], std::to_string(client_port0));

        //Check payload
        CHECK_EQ(actual_payload.size(), 0);



        // Erase key from node 0, expect ask response
        processed = std::async(process_instruction, std::ref(server0), client_port0);
        std::this_thread::sleep_for(100ms);
        sent = std::async(send_instruction, client_port0, protocol::Command{ other_key_slot_0 }, protocol::Instruction::c_ERASE, "");
        std::tie(actual_metadata, actual_command, actual_payload) = sent.get();
        processed.get();

        CHECK_EQ(1, server0.get_kvs().get_size());
        CHECK_EQ(1, server0.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(1, server1.get_kvs().get_size());
        CHECK_EQ(1, server1.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(SlotState::c_MIGRATING, server0.get_cluster_state().slots[0].state);
        CHECK_EQ(SlotState::c_IMPORTING, server1.get_cluster_state().slots[0].state);

        //Check metadata
        CHECK_EQ(actual_metadata.argc, 2);
        CHECK_EQ(actual_metadata.command_size, 8 + 9 + 8 + 4);
        CHECK_EQ(actual_metadata.payload_size, 0);
        CHECK_EQ(actual_metadata.instruction, protocol::Instruction::c_ASK);

        //Check command
        CHECK_EQ(actual_command.size(), 2);
        CHECK_EQ(actual_command[0], "127.0.0.1");
        CHECK_EQ(actual_command[1], std::to_string(client_port1));

        //Check payload
        CHECK_EQ(actual_payload.size(), 0);



        // Erase only key from node0, expect ok and slot state change to normal and not served by node0
        processed = std::async(process_instruction, std::ref(server0), client_port0);
        std::this_thread::sleep_for(100ms);
        sent = std::async(send_instruction, client_port0, protocol::Command{ key_slot_0 }, protocol::Instruction::c_ERASE, "");
        std::tie(actual_metadata, actual_command, actual_payload) = sent.get();
        processed.get();

        server1.handle_connection(connection1);

        CHECK_EQ(0, server0.get_kvs().get_size());
        CHECK_EQ(0, server0.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(1, server1.get_kvs().get_size());
        CHECK_EQ(1, server1.get_cluster_state().slots[0].amount_of_keys);
        CHECK_EQ(SlotState::c_NORMAL, server0.get_cluster_state().slots[0].state);
        CHECK_EQ(SlotState::c_NORMAL, server1.get_cluster_state().slots[0].state);

        CHECK_FALSE(server0.get_cluster_state().myself.served_slots[0]);
        CHECK_EQ(0, server0.get_cluster_state().myself.num_slots_served);
        CHECK_EQ(nullptr, server0.get_cluster_state().slots[0].migration_partner);
        CHECK_EQ(nullptr, server1.get_cluster_state().slots[0].migration_partner);

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
}


TEST_CASE("Test get slots") {
    uint16_t cluster_port = 3000, client_port = 4000;
    Node server = Node::new_in_memory_node("node", client_port, cluster_port, "127.0.0.1");
    cluster::ClusterState& state = server.get_cluster_state();

    cluster::ClusterNode node1{ "node1", "127.0.0.1", 4000, 3001 };
    cluster::ClusterNode node2{ "node2", "127.0.0.1", 4000, 3002 };
    cluster::ClusterNode node3{ "node3", "127.0.0.100", 4000, 3003 };

    state.slots.resize(5);
    state.slots[1].served_by = &node1;
    state.slots[2].served_by = &node2;
    state.slots[3].served_by = &node3;

    auto send_instruction = [&cluster_port]() {
        auto connection = net::Socket{}.connect(cluster_port);
        protocol::send_instruction(connection, protocol::Command{}, protocol::Instruction::c_GET_SLOTS, "");

        auto received_metadata = protocol::get_metadata(connection);
        auto received_command = protocol::get_command(connection, received_metadata.argc, received_metadata.command_size);
        ByteArray received_payload = protocol::get_payload(connection, received_metadata.payload_size);

        return std::make_tuple(received_metadata, received_command, received_payload);
    };

    auto handle_request = [&cluster_port, &server]() {
        net::Socket socket{};
        if (!net::is_listening(socket.fd())) {
            socket.listen(cluster_port);
        }

        net::Connection connection = socket.accept();
        server.handle_connection(connection);
    };


    auto processed = std::async(handle_request);
    std::this_thread::sleep_for(100ms);
    auto sent = std::async(send_instruction);
    auto [actual_metadata, actual_command, actual_payload] = sent.get();
    processed.get();

    //Check metadata
    CHECK_EQ(actual_metadata.argc, 0);
    CHECK_EQ(actual_metadata.command_size, 0);
    CHECK_EQ(actual_metadata.payload_size, 76);
    CHECK_EQ(actual_metadata.instruction, protocol::Instruction::c_OK_RESPONSE);

    //Check command
    CHECK_EQ(actual_command.size(), 0);

    //Check payload
    CHECK_EQ(actual_payload.size(), 76);
    //0 0 NULL 1 1 127.0.0.1:3001 2 2 127.0.0.1:3002 3 3 127.0.0.100:3003 4 4 NULL
    std::string expected_payload = "0\t0\tNULL\n1\t1\t127.0.0.1:3001\n2\t2\t127.0.0.1:3002\n3\t3\t127.0.0.100:3003\n4\t4\tNULL";
    CHECK_EQ(actual_payload.to_string(), expected_payload);
}
*/
