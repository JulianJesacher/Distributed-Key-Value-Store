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
    int client_port = 8080, cluster_port = 8081;
    Client client{};
    Node node{ client_port, cluster_port };

    auto thread = std::thread([&node]() {
        node.start();
        });
    std::this_thread::sleep_for(100ms);

    SUBCASE("No connections open") {
        //Initially, no connections are open
        CHECK(node.get_connections_epoll().wait(0) == 0);
        CHECK_EQ(client.get_nodes_connections().size(), 0);
    }

    SUBCASE("Client connect") {
        //After connecting to a node, a connection should be open
        client.connect_to_node("127.0.0.1", client_port);
        CHECK(node.get_connections_epoll().wait(0) == 1);
        CHECK_EQ(client.get_nodes_connections().size(), 1);
    }

    SUBCASE("Socket connect") {
        //Also connections from a socket should be accepted
        net::Socket client_socket{};
        client_socket.connect(client_port);
        CHECK(node.get_connections_epoll().wait(0) == 1);
    }

    node.stop();
    thread.join();
}


TEST_CASE("Test put") {
    uint16_t client_port0 = 8080, cluster_port0 = 8081;
    uint16_t client_port1 = 8082, cluster_port1 = 8083;
    Node node0{ client_port0, cluster_port0 };
    Node node1{ client_port1, cluster_port1 };
    ClusterNode cluster_node1{ "node1", "127.0.0.1", cluster_port1, client_port1 };
    Client client{};

    for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
        node0.get_cluster_state().slots[i].served_by = &cluster_node1;
    }

    auto thread0 = std::thread([&node0]() {
        node0.start();
        });
    std::this_thread::sleep_for(100ms);

    client.connect_to_node("127.0.0.1", client_port0);
    CHECK_EQ(0, node0.get_kvs().get_size());

    SUBCASE("Put new value") {
        for (int i = 0; i < cluster::CLUSTER_AMOUNT_OF_SLOTS; i++) {
            node0.get_cluster_state().myself.served_slots[i] = true;
        }

        auto status = client.put_value("key", "value");
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
        auto status = client.put_value("key", "valuevalue", 10, 5);
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

        auto status = client.put_value("key", "value");
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

        //Start node1
        auto thread1 = std::thread([&node1]() {
            node1.start();
            });
        std::this_thread::sleep_for(100ms);

        auto status = client.put_value("key", "value");
        CHECK(status.is_ok());
        CHECK_EQ(0, node0.get_kvs().get_size());
        CHECK_EQ(1, node1.get_kvs().get_size());
        CHECK(!node0.get_kvs().contains_key("key"));
        CHECK(node1.get_kvs().contains_key("key"));

        ByteArray actual_value = ByteArray::new_allocated_byte_array(5);
        node1.get_kvs().get("key", actual_value);
        CHECK_EQ("value", actual_value.to_string());

        //Stop node1
        node1.stop();
        thread1.join();
    }

    node0.stop();
    thread0.join();
}