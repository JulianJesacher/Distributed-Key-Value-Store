#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <thread>
#include <chrono>
#include <sys/epoll.h>

#include "client/Client.hpp"
#include "node/node.hpp"
#include "net/Socket.hpp"
#include "net/Connection.hpp"

using namespace client;
using namespace node;
using namespace node::cluster;
using namespace std::chrono_literals;


TEST_CASE("Test connect") {
    std::cout << "Test connect" << std::endl;

    int client_port0 = 8080, cluster_port0 = 8081;
    Client client0{};
    Node node0 = Node::new_in_memory_node("node0", client_port0, cluster_port0, "127.0.0.1");

    auto thread0 = std::thread(&Node::start, &node0);
    std::this_thread::sleep_for(100ms);

    SUBCASE("No connections open") {
        //Initially, no connections are open
        CHECK(node0.get_connections_epoll().wait(0) == 0);
        CHECK_EQ(client0.get_nodes_connections().size(), 0);
    }

    SUBCASE("Client connect") {
        //After connecting to a node, a connection should be open
        client0.connect_to_node("127.0.0.1", client_port0);
        CHECK(node0.get_connections_epoll().wait(0) == 1); //TODO: REMOVE
        CHECK_EQ(client0.get_nodes_connections().size(), 1);
    }

    SUBCASE("Socket connect") {
        //Also connections from a socket should be accepted
        net::Socket client_socket{};
        client_socket.connect(client_port0);
        CHECK(node0.get_connections_epoll().wait(0) == 1);
    }

    //TODO: Fix
    //SUBCASE("Server terminates connection") {
    //    //When the client connects to the node and the node stops, a subsequent receive call on the opened connection should return -1
    //    CHECK(client0.connect_to_node("127.0.0.1", client_port0));
    //    auto& connection = client0.get_nodes_connections()["127.0.0.1:" + std::to_string(client_port0)];
    //
    //    node0.stop();
    //    thread0.join();
    //    std::this_thread::sleep_for(100ms);
    //
    //    char buf[1];
    //    CHECK(connection.receive(buf, 1) == -1);
    //}

    node0.stop();
    if (thread0.joinable()) {
        thread0.join();
    }
}


TEST_CASE("Test put") {
    std::cout << "Test put" << std::endl;

    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0 = Node::new_in_memory_node("node0", client_port0, cluster_port0, "127.0.0.1");
    Node node1 = Node::new_in_memory_node("node1", client_port1, cluster_port1, "127.0.0.1");
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{}, client1{};

    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
        node0.get_cluster_state().slots[i].served_by = &cluster_node1;
    }

    auto thread0 = std::thread(&Node::start, &node0);
    auto thread1 = std::thread(&Node::start, &node1);
    std::this_thread::sleep_for(100ms);

    CHECK(client0.connect_to_node("127.0.0.1", client_port0));
    CHECK(client1.connect_to_node("127.0.0.1", client_port1));
    CHECK_EQ(0, node0.get_kvs().get_size());


    SUBCASE("Put new value") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
        }

        auto status = client0.put_value("key", "value");
        CHECK(status.is_ok());
        CHECK_EQ(1, node0.get_kvs().get_size());
        CHECK(node0.get_kvs().contains_key("key"));

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        node0.get_kvs().get("key", actual_value);
        CHECK_EQ("value", actual_value.to_string());
    }

    SUBCASE("Overwrite existing value") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
        }

        //Put a value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node0.get_kvs().put("key", value);

        //Update the value by appending
        auto status = client0.put_value("key", "valuevalue", 10, 5);
        CHECK(status.is_ok());
        CHECK_EQ(1, node0.get_kvs().get_size());
        CHECK(node0.get_kvs().contains_key("key"));

        ByteArray actual_value = ByteArray::new_allocated_byte_array(15);
        node0.get_kvs().get("key", actual_value);
        CHECK_EQ("valuevaluevalue", actual_value.to_string());
    }

    SUBCASE("Call put on wrong node, not successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = false;
            node1.get_cluster_state().myself.served_slots[i] = true;
        }

        //Stop node1
        node1.stop();
        thread1.join();

        auto status = client0.put_value("key", "value");
        CHECK(status.is_error());
        CHECK_EQ(0, node0.get_kvs().get_size());
        CHECK_EQ(0, node1.get_kvs().get_size());
        CHECK_EQ("Could not connect to new node", status.get_msg());
    }

    SUBCASE("Call put on wrong node, successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = false;
            node1.get_cluster_state().myself.served_slots[i] = true;

            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }

        auto status = client0.put_value("key", "value");
        CHECK(status.is_ok());
        CHECK_EQ(0, node0.get_kvs().get_size());
        CHECK_EQ(1, node1.get_kvs().get_size());
        CHECK(!node0.get_kvs().contains_key("key"));
        CHECK(node1.get_kvs().contains_key("key"));

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        node1.get_kvs().get("key", actual_value);
        CHECK_EQ("value", actual_value.to_string());

        //Check if moved was successful
        uint16_t slot = cluster::get_key_hash("key") % cluster::CLUSTER_AMOUNT_OF_SLOTS;
        std::string expected_node = "127.0.0.1:" + std::to_string(client_port1);
        CHECK_EQ(expected_node, client0.get_slot_nodes()[slot]);
    }

    SUBCASE("Slot migrating, ask returned") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_MIGRATING;
            node0.get_cluster_state().slots[i].migration_partner = &cluster_node1;

            node1.get_cluster_state().myself.served_slots[i] = true;
            node1.get_cluster_state().slots[i].state = cluster::SlotState::c_IMPORTING;
            node1.get_cluster_state().slots[i].migration_partner = &cluster_node1;
        }

        auto status = client0.put_value("key", "value");
        CHECK(status.is_ok());
        CHECK_EQ(0, node0.get_kvs().get_size());
        CHECK_EQ(1, node1.get_kvs().get_size());
        CHECK(!node0.get_kvs().contains_key("key"));
        CHECK(node1.get_kvs().contains_key("key"));

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        node1.get_kvs().get("key", actual_value);
        CHECK_EQ("value", actual_value.to_string());
    }

    //Stop nodes
    node0.stop();
    node1.stop();
    if (thread0.joinable()) {
        thread0.join();
    }
    if (thread1.joinable()) {
        thread1.join();
    }
}


TEST_CASE("Test get") {
    std::cout << "Test get" << std::endl;

    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0 = Node::new_in_memory_node("node0", client_port0, cluster_port0, "127.0.0.1");
    Node node1 = Node::new_in_memory_node("node1", client_port1, cluster_port1, "127.0.0.1");
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{}, client1{};

    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
        node0.get_cluster_state().slots[i].served_by = &cluster_node1;
    }

    auto thread0 = std::thread(&Node::start, &node0);
    auto thread1 = std::thread(&Node::start, &node1);
    std::this_thread::sleep_for(100ms);

    CHECK(client0.connect_to_node("127.0.0.1", client_port0));
    CHECK(client1.connect_to_node("127.0.0.1", client_port1));
    CHECK_EQ(0, node0.get_kvs().get_size());


    SUBCASE("Get value from empty kvs") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
        }

        ByteArray value = ByteArray::new_allocated_byte_array(0);
        auto status = client0.get_value("key", value);
        CHECK(status.is_error());
        CHECK_EQ("The given key was not found", status.get_msg());
    }

    SUBCASE("Get value normal") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
        }

        //Put a value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node0.get_kvs().put("key", value);

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        auto status = client0.get_value("key", actual_value);
        CHECK(status.is_ok());
        CHECK_EQ("value", actual_value.to_string());
    }

    SUBCASE("Get value with offset and different sizes") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
        }

        //Put a value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node0.get_kvs().put("key", value);

        ByteArray actual_value = ByteArray::new_allocated_byte_array(0);
        auto status = client0.get_value("key", actual_value, 1, 2);
        CHECK(status.is_ok());
        CHECK_EQ("al", actual_value.to_string().substr(1, 2));
        CHECK_EQ(5, actual_value.size());
    }

    SUBCASE("Get value from wrong node, not successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = false;
            node1.get_cluster_state().myself.served_slots[i] = true;
        }

        //Stop node1
        node1.stop();
        thread1.join();

        ByteArray value = ByteArray::new_allocated_byte_array(0);
        auto status = client0.get_value("key", value);
        CHECK(status.is_error());
        CHECK_EQ("Could not connect to new node", status.get_msg());
    }

    SUBCASE("Get value from wrong node, successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = false;
            node1.get_cluster_state().myself.served_slots[i] = true;

            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }

        //Put a value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node1.get_kvs().put("key", value);

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        auto status = client0.get_value("key", actual_value);
        CHECK(status.is_ok());
        CHECK_EQ("value", actual_value.to_string());

        //Check if moved was successful
        uint16_t slot = cluster::get_key_hash("key") % cluster::CLUSTER_AMOUNT_OF_SLOTS;
        std::string expected_node = "127.0.0.1:" + std::to_string(client_port1);
        CHECK_EQ(expected_node, client0.get_slot_nodes()[slot]);
    }

    SUBCASE("Get from migrating slot, connect to migrating node") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_MIGRATING;
            node0.get_cluster_state().slots[i].migration_partner = &cluster_node1;

            node1.get_cluster_state().myself.served_slots[i] = true;
            node1.get_cluster_state().slots[i].state = cluster::SlotState::c_IMPORTING;
            node1.get_cluster_state().slots[i].migration_partner = &cluster_node0;
        }

        //Put a value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node1.get_kvs().put("key", value);

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        auto status = client0.get_value("key", actual_value);
        CHECK(status.is_ok());
        CHECK_EQ("value", actual_value.to_string());
    }

    SUBCASE("Get from migrating slot, connect to importing node, not successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_MIGRATING;
            node0.get_cluster_state().slots[i].migration_partner = &cluster_node1;

            node1.get_cluster_state().myself.served_slots[i] = true;
            node1.get_cluster_state().slots[i].state = cluster::SlotState::c_IMPORTING;
            node1.get_cluster_state().slots[i].migration_partner = &cluster_node0;
        }

        //Stop node0, should not work because of asking
        node0.stop();
        thread0.join();

        //Put a value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node1.get_kvs().put("key", value);

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        auto status = client1.get_value("key", actual_value);
        CHECK(status.is_error());
        CHECK_EQ("Could not connect to new node", status.get_msg());
    }

    SUBCASE("Get from migrating slot, connect to importing node, successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_MIGRATING;
            node0.get_cluster_state().slots[i].migration_partner = &cluster_node1;

            node1.get_cluster_state().myself.served_slots[i] = true;
            node1.get_cluster_state().slots[i].state = cluster::SlotState::c_IMPORTING;
            node1.get_cluster_state().slots[i].migration_partner = &cluster_node0;
        }

        //Put a value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node1.get_kvs().put("key", value);

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        auto status = client1.get_value("key", actual_value);
        CHECK(status.is_ok());
        CHECK_EQ("value", actual_value.to_string());
    }

    //Stop nodes
    node0.stop();
    node1.stop();
    if (thread0.joinable()) {
        thread0.join();
    }
    if (thread1.joinable()) {
        thread1.join();
    }
}


TEST_CASE("Test erase") {
    std::cout << "Test erase" << std::endl;

    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0 = Node::new_in_memory_node("node0", client_port0, cluster_port0, "127.0.0.1");
    Node node1 = Node::new_in_memory_node("node1", client_port1, cluster_port1, "127.0.0.1");
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{}, client1{};

    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
        node0.get_cluster_state().slots[i].served_by = &cluster_node1;
    }

    auto thread0 = std::thread(&Node::start, &node0);
    auto thread1 = std::thread(&Node::start, &node1);
    std::this_thread::sleep_for(100ms);

    CHECK(client0.connect_to_node("127.0.0.1", client_port0));
    CHECK(client1.connect_to_node("127.0.0.1", client_port1));


    SUBCASE("Erase not existing value") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
        }

        auto status = client0.erase_value("key");
        CHECK(status.is_error());
        CHECK_EQ("The given key was not found", status.get_msg());
    }

    SUBCASE("Erase normal behaviour") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
        }

        //Put value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node0.get_kvs().put("key", value);

        auto status = client0.erase_value("key");
        CHECK(status.is_ok());
        CHECK_EQ(0, node0.get_kvs().get_size());
        CHECK_EQ(0, status.get_msg().size());
    }

    SUBCASE("Connect to wrong node, successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = false;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_NORMAL;
            node0.get_cluster_state().slots[i].served_by = &cluster_node1;

            node1.get_cluster_state().myself.served_slots[i] = true;
            node1.get_cluster_state().slots[i].state = cluster::SlotState::c_NORMAL;

            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }

        //Put value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node1.get_kvs().put("key", value);

        auto status = client0.erase_value("key");
        CHECK(status.is_ok());
        CHECK_EQ(0, node1.get_kvs().get_size());
        CHECK_EQ(0, status.get_msg().size());

        //Check if moved was successful
        uint16_t slot = cluster::get_key_hash("key") % cluster::CLUSTER_AMOUNT_OF_SLOTS;
        std::string expected_node = "127.0.0.1:" + std::to_string(client_port1);
        CHECK_EQ(expected_node, client0.get_slot_nodes()[slot]);
    }

    SUBCASE("Migrating slot, successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_MIGRATING;
            node0.get_cluster_state().slots[i].migration_partner = &cluster_node1;

            node1.get_cluster_state().myself.served_slots[i] = true;
            node1.get_cluster_state().slots[i].state = cluster::SlotState::c_IMPORTING;
            node1.get_cluster_state().slots[i].migration_partner = &cluster_node0;
        }

        //Put value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node1.get_kvs().put("key", value);

        //Check valid when connecting to migrating node
        auto status = client0.erase_value("key");
        CHECK(status.is_ok());
        CHECK_EQ(0, node1.get_kvs().get_size());
        CHECK_EQ(0, status.get_msg().size());

        //Put value again
        node1.get_kvs().put("key", value);

        //Check valid when connecting to importing node
        status = client1.erase_value("key");
        CHECK(status.is_ok());
        CHECK_EQ(0, node1.get_kvs().get_size());
        CHECK_EQ(0, status.get_msg().size());
    }

    SUBCASE("Migrating slot, not successful") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_MIGRATING;
            node0.get_cluster_state().slots[i].migration_partner = &cluster_node1;

            node1.get_cluster_state().myself.served_slots[i] = true;
            node1.get_cluster_state().slots[i].state = cluster::SlotState::c_IMPORTING;
            node1.get_cluster_state().slots[i].migration_partner = &cluster_node0;
        }

        //Stop node1
        node1.stop();
        thread1.join();

        //Put value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node1.get_kvs().put("key", value);


        auto status = client0.erase_value("key");
        CHECK(status.is_error());
        CHECK_EQ(1, node1.get_kvs().get_size());
        CHECK_EQ("Could not connect to new node", status.get_msg());
    }

    //Stop nodes
    node0.stop();
    node1.stop();
    if (thread0.joinable()) {
        thread0.join();
    }
    if (thread1.joinable()) {
        thread1.join();
    }
}

TEST_CASE("Test update slot info") {
    std::cout << "Test update slot info" << std::endl;

    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    uint16_t client_port2 = 8084, cluster_port2 = 8085;
    Node node0 = Node::new_in_memory_node("node0", client_port0, cluster_port0, "127.0.0.1");
    Node node1 = Node::new_in_memory_node("node1", client_port1, cluster_port1, "127.0.0.1");
    Node node2 = Node::new_in_memory_node("node2", client_port2, cluster_port2, "127.0.0.1");
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    ClusterNode cluster_node2{ "node2", "127.0.0.1", cluster_port2, client_port2 };
    Client client0{}, client1{}, client2{};

    //Start nodes
    auto thread0 = std::thread(&Node::start, &node0);
    auto thread1 = std::thread(&Node::start, &node1);
    auto thread2 = std::thread(&Node::start, &node2);
    std::this_thread::sleep_for(100ms);

    CHECK(client0.connect_to_node("127.0.0.1", client_port0));
    CHECK(client1.connect_to_node("127.0.0.1", client_port1));
    CHECK(client2.connect_to_node("127.0.0.1", client_port2));

    SUBCASE("Test no connection") {
        client0.disconnect_all();
        client1.disconnect_all();
        client2.disconnect_all();

        auto status = client0.get_update_slot_info();
        CHECK(status.is_error());
        CHECK_EQ("Not connected to any node", status.get_msg());
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            CHECK_EQ("", client0.get_slot_nodes()[i]);
        }
    }

    SUBCASE("Test interval") {
        node0.get_cluster_state().slots[0].served_by = &cluster_node0;
        node0.get_cluster_state().slots[1].served_by = &cluster_node0;

        auto status = client0.get_update_slot_info();
        CHECK(status.is_ok());
        CHECK_EQ(0, status.get_msg().size());

        CHECK_EQ("127.0.0.1:" + std::to_string(client_port0), client0.get_slot_nodes()[0]);
        CHECK_EQ("127.0.0.1:" + std::to_string(client_port0), client0.get_slot_nodes()[1]);
        for (int i = 2; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            CHECK_EQ("", client0.get_slot_nodes()[i]);
        }
    }

    //Stop nodes
    node0.stop();
    node1.stop();
    node2.stop();
    if (thread0.joinable()) {
        thread0.join();
    }
    if (thread1.joinable()) {
        thread1.join();
    }
    if (thread2.joinable()) {
        thread2.join();
    }
}


void compare_cluster_node(const ClusterNode& node1, const ClusterNode& node2) {
    CHECK_EQ(node1.name, node2.name);
    CHECK_EQ(node1.ip, node2.ip);
    CHECK_EQ(node1.cluster_port, node2.cluster_port);
    CHECK_EQ(node1.client_port, node2.client_port);
    CHECK_EQ(node1.served_slots, node2.served_slots);
    CHECK_EQ(node1.num_slots_served, node2.num_slots_served);
}


TEST_CASE("Test migrate slot") {
    std::cout << "Test migrate slot" << std::endl;

    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0 = Node::new_in_memory_node("node0", client_port0, cluster_port0, "127.0.0.1");
    Node node1 = Node::new_in_memory_node("node1", client_port1, cluster_port1, "127.0.0.1");
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{}, client1{};

    //Start nodes
    auto thread0 = std::thread(&Node::start, &node0);
    auto thread1 = std::thread(&Node::start, &node1);
    std::this_thread::sleep_for(100ms);

    CHECK(client0.connect_to_node("127.0.0.1", client_port0));
    CHECK(client1.connect_to_node("127.0.0.1", client_port1));

    SUBCASE("Other node not in cluster") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_NORMAL;
            node0.get_cluster_state().slots[i].migration_partner = nullptr;
            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }
        client0.disconnect_all();

        auto status = client0.migrate_slot(0, "127.0.0.1", client_port1);
        CHECK(status.is_error());
        CHECK_EQ("Other node not part of the cluster", status.get_msg());
    }

    SUBCASE("Migrating slot has no keys") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_NORMAL;
            node0.get_cluster_state().slots[i].migration_partner = nullptr;
            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }

        net::Socket socket{};
        net::Connection cluster_bus = socket.connect("127.0.0.1", cluster_port1);
        cluster_node1.outgoing_link = cluster_bus;
        node0.get_cluster_state().nodes["node1"] = cluster_node1;

        auto status = client0.migrate_slot(0, "127.0.0.1", client_port1);
        CHECK(status.is_ok());
        CHECK_EQ(0, status.get_msg().size());

        //Because the slot has no keys, it should be migrated immediately
        CHECK_EQ(cluster::SlotState::c_NORMAL, node0.get_cluster_state().slots[0].state);
        CHECK_EQ(nullptr, node0.get_cluster_state().slots[0].migration_partner);
    }

    SUBCASE("Normal behavior") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_NORMAL;
            node0.get_cluster_state().slots[i].migration_partner = nullptr;
            node0.get_cluster_state().slots[i].amount_of_keys = 1;
            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }

        net::Socket socket{};
        net::Connection cluster_bus = socket.connect("127.0.0.1", cluster_port1);
        cluster_node1.outgoing_link = cluster_bus;
        node0.get_cluster_state().nodes["node1"] = cluster_node1;

        auto status = client0.migrate_slot(0, "127.0.0.1", client_port1);
        CHECK(status.is_ok());
        CHECK_EQ(0, status.get_msg().size());

        CHECK_EQ(cluster::SlotState::c_MIGRATING, node0.get_cluster_state().slots[0].state);
        compare_cluster_node(cluster_node1, *node0.get_cluster_state().slots[0].migration_partner);
    }

    //Stop nodes
    node0.stop();
    node1.stop();
    if (thread0.joinable()) {
        thread0.join();
    }
    if (thread1.joinable()) {
        thread1.join();
    }
}


TEST_CASE("Test import slot") {
    std::cout << "Test import slot" << std::endl;

    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0 = Node::new_in_memory_node("node0", client_port0, cluster_port0, "127.0.0.1");
    Node node1 = Node::new_in_memory_node("node1", client_port1, cluster_port1, "127.0.0.1");
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{}, client1{};

    //Start nodes
    auto thread0 = std::thread(&Node::start, &node0);
    auto thread1 = std::thread(&Node::start, &node1);
    std::this_thread::sleep_for(100ms);

    CHECK(client0.connect_to_node("127.0.0.1", client_port0));
    CHECK(client1.connect_to_node("127.0.0.1", client_port1));

    SUBCASE("Other node not in cluster") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_NORMAL;
            node0.get_cluster_state().slots[i].migration_partner = nullptr;
            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }
        client0.disconnect_all();

        auto status = client0.import_slot(0, "127.0.0.1", client_port1);
        CHECK(status.is_error());
        CHECK_EQ("Other node not part of the cluster", status.get_msg());
    }

    SUBCASE("Importing slot has no keys") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_NORMAL;
            node0.get_cluster_state().slots[i].migration_partner = nullptr;
            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }

        net::Socket socket{};
        net::Connection cluster_bus = socket.connect("127.0.0.1", cluster_port1);
        cluster_node1.outgoing_link = cluster_bus;
        node0.get_cluster_state().nodes["node1"] = cluster_node1;

        auto status = client0.import_slot(0, "127.0.0.1", client_port1);
        CHECK(status.is_ok());
        CHECK_EQ(0, status.get_msg().size());

        //Importing also works with 0 keys
        CHECK_EQ(cluster::SlotState::c_IMPORTING, node0.get_cluster_state().slots[0].state);
        compare_cluster_node(cluster_node1, *node0.get_cluster_state().slots[0].migration_partner);
    }

    SUBCASE("Normal behavior") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
            node0.get_cluster_state().slots[i].state = cluster::SlotState::c_NORMAL;
            node0.get_cluster_state().slots[i].migration_partner = nullptr;
            node0.get_cluster_state().slots[i].amount_of_keys = 1;
            client0.get_slot_nodes()[i] = "127.0.0.1:" + std::to_string(client_port0);
        }

        net::Socket socket{};
        net::Connection cluster_bus = socket.connect("127.0.0.1", cluster_port1);
        cluster_node1.outgoing_link = cluster_bus;
        node0.get_cluster_state().nodes["node1"] = cluster_node1;

        auto status = client0.migrate_slot(0, "127.0.0.1", client_port1);
        CHECK(status.is_ok());
        CHECK_EQ(0, status.get_msg().size());

        CHECK_EQ(cluster::SlotState::c_MIGRATING, node0.get_cluster_state().slots[0].state);
        compare_cluster_node(cluster_node1, *node0.get_cluster_state().slots[0].migration_partner);
    }

    //Stop nodes
    node0.stop();
    node1.stop();
    if (thread0.joinable()) {
        thread0.join();
    }
    if (thread1.joinable()) {
        thread1.join();
    }
}


TEST_CASE("Test add node") {
    std::cout << "Test add node" << std::endl;

    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0 = Node::new_in_memory_node("node0", client_port0, cluster_port0, "127.0.0.1");
    Node node1 = Node::new_in_memory_node("node1", client_port1, cluster_port1, "127.0.0.1");
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{};

    //Start nodes
    auto thread0 = std::thread{ &Node::start, &node0 };
    auto thread1 = std::thread{ &Node::start, &node1 };
    std::this_thread::sleep_for(100ms);

    CHECK(client0.connect_to_node("127.0.0.1", client_port0));

    SUBCASE("Normal") {
        auto status = client0.add_node_to_cluster("node1", "127.0.0.1", client_port1, cluster_port1);

        CHECK(status.is_ok());
        CHECK_EQ(0, status.get_msg().size());

        CHECK_EQ(1, node0.get_cluster_state().nodes.size());
        auto actual_node = node0.get_cluster_state().nodes["node1"];
        std::string expected_name("node1");

        CHECK(memcmp(expected_name.data(), actual_node.name.data(), expected_name.size()) == 0);
        CHECK_EQ(client_port1, actual_node.client_port);
        CHECK_EQ(cluster_port1, actual_node.cluster_port);
    }

    SUBCASE("Node already in cluster") {
        auto status = client0.add_node_to_cluster("node1", "127.0.0.1", client_port1, cluster_port1);
        CHECK(status.is_ok());

        status = client0.add_node_to_cluster("node1", "127.0.0.1", client_port1, cluster_port1);
        CHECK(status.is_error());
        CHECK_EQ("Node with name node1 already in cluster", status.get_msg());
    }

    //Stop nodes
    node0.stop();
    node1.stop();
    if (thread0.joinable()) {
        thread0.join();
    }
    if (thread1.joinable()) {
        thread1.join();
    }
}
