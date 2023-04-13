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
    int client_port0 = 8080, cluster_port0 = 8081;
    Client client0{};
    Node node0{ client_port0, cluster_port0 };

    auto thread0 = std::thread([&node0]() {
        node0.start();
        });
    std::this_thread::sleep_for(100ms);

    SUBCASE("No connections open") {
        //Initially, no connections are open
        CHECK(node0.get_connections_epoll().wait(0) == 0);
        CHECK_EQ(client0.get_nodes_connections().size(), 0);
    }

    SUBCASE("Client connect") {
        //After connecting to a node, a connection should be open
        client0.connect_to_node("127.0.0.1", client_port0);
        CHECK(node0.get_connections_epoll().wait(0) == 1);
        CHECK_EQ(client0.get_nodes_connections().size(), 1);
    }

    SUBCASE("Socket connect") {
        //Also connections from a socket should be accepted
        net::Socket client_socket{};
        client_socket.connect(client_port0);
        CHECK(node0.get_connections_epoll().wait(0) == 1);
    }

    node0.stop();
    thread0.join();
}


TEST_CASE("Test put") {
    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0{ client_port0, cluster_port0 };
    Node node1{ client_port1, cluster_port1 };
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{}, client1{};

    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
        node0.get_cluster_state().slots[i].served_by = &cluster_node1;
    }

    auto thread0 = std::thread([&node0]() {
        node0.start();
        });
    auto thread1 = std::thread([&node1]() {
        node1.start();
        });
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


TEST_CASE("Get value") {
    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0{ client_port0, cluster_port0 };
    Node node1{ client_port1, cluster_port1 };
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{}, client1{};

    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
        node0.get_cluster_state().slots[i].served_by = &cluster_node1;
    }

    auto thread0 = std::thread([&node0]() {
        node0.start();
        });
    auto thread1 = std::thread([&node1]() {
        node1.start();
        });
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
        }

        //Put a value
        ByteArray value = ByteArray::new_allocated_byte_array("value");
        node1.get_kvs().put("key", value);

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        auto status = client0.get_value("key", actual_value);
        CHECK(status.is_ok());
        CHECK_EQ("value", actual_value.to_string());
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


TEST_CASE("Test erase value") {
    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0{ client_port0, cluster_port0 };
    Node node1{ client_port1, cluster_port1 };
    ClusterNode cluster_node0{ "node0", "127.0.0.1", cluster_port0, client_port0 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client0{}, client1{};

    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
        node0.get_cluster_state().slots[i].served_by = &cluster_node1;
    }

    auto thread0 = std::thread([&node0]() {
        node0.start();
        });
    auto thread1 = std::thread([&node1]() {
        node1.start();
        });
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

    //TODO: Moved for get and put
    SUBCASE("Moved") {
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

        //Check that moved has been stored
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

