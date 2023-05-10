#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "node/node.hpp"
#include "client/Client.hpp"
#include "node/InstructionHandler.hpp"
#include "node/Cluster.hpp"
#include "net/Socket.hpp"
#include "net/Connection.hpp"

using namespace node;
using namespace client;


std::string get_key_with_target_slot(uint16_t slot, std::vector<std::string> distinct = {}) {
    std::string key = "key";
    while (cluster::get_key_hash(key) % cluster::CLUSTER_AMOUNT_OF_SLOTS != slot
        || std::find(distinct.begin(), distinct.end(), key) != distinct.end()) {
        key += "1";
    }
    return key;
}

TEST_CASE("Integration test") {
    //Initialize 3 nodes
    uint16_t client_port0{ 15000 }, client_port1{ 15001 }, client_port2{ 15002 };
    uint16_t cluster_port0{ 16000 }, cluster_port1{ 16001 }, cluster_port2{ 16002 };
    std::string node_name0{"node0"}, node_name1{ "node1" }, node_name2{ "node2" };
    std::string node_ip0{"127.0.0.1"}, node_ip1{ "127.0.0.1" }, node_ip2{ "127.0.0.1" };
    bool serve_all_slots0{ true }, serve_all_slots1{ false }, serve_all_slots2{ false };

    Node node0 = Node::new_in_memory_node(node_name0, client_port0, cluster_port0, node_ip0, serve_all_slots0);
    Node node1 = Node::new_in_memory_node(node_name1, client_port1, cluster_port1, node_ip1, serve_all_slots1);
    Node node2 = Node::new_in_memory_node(node_name2, client_port2, cluster_port2, node_ip2, serve_all_slots2);

    Client client0{};
    Client client1{};

    //Start nodes in separate threads
    std::thread thread0(&Node::start, &node0);
    std::thread thread1(&Node::start, &node1);
    std::thread thread2(&Node::start, &node2);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    //Connect client to node
    Status status = client0.connect_to_node(node_ip0, client_port0);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());

    //Add nodes to cluster
    status = client0.add_node_to_cluster(node_name1, node_ip1, client_port1, cluster_port1);
    CHECK(status.is_ok());
    status = client0.add_node_to_cluster(node_name2, node_ip2, client_port2, cluster_port2);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());

    //Wait for cluster to be formed
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    //Update slot info
    status = client0.get_update_slot_info();
    CHECK_MESSAGE(status.is_ok(), status.get_msg());

    //Put value with slot0, should end up in node0
    std::string key0_0 = get_key_with_target_slot(0);
    std::string value0_0 = "value0_0";
    status = client0.put_value(key0_0, value0_0);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());
    CHECK_EQ(1, node0.get_kvs().get_size());
    CHECK_EQ(1, node0.get_cluster_state().slots[0].amount_of_keys);

    //Migrate slot0 to node1
    status = client0.migrate_slot(0, node_ip1, client_port1);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());
    CHECK_EQ(cluster::SlotState::c_MIGRATING, node0.get_cluster_state().slots[0].state);

    //Import slot0 from node0 to node1
    status = client0.import_slot(0, node_ip1, client_port1);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());
    CHECK_EQ(cluster::SlotState::c_IMPORTING, node1.get_cluster_state().slots[0].state);

    //Put another value with slot0, should end up in node1
    std::string key0_1 = get_key_with_target_slot(0, { key0_0 });
    std::string value0_1 = "value0_1";
    status = client0.put_value(key0_1, value0_1);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());
    CHECK_EQ(1, node0.get_kvs().get_size());
    CHECK_EQ(1, node1.get_kvs().get_size());

    //Get both values
    ByteArray byte_array{};
    status = client0.get_value(key0_0, byte_array);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());
    CHECK_EQ(value0_0, byte_array.to_string());
    status = client0.get_value(key0_1, byte_array);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());
    CHECK_EQ(value0_1, byte_array.to_string());

    //Erase old value
    status = client0.erase_value(key0_0);
    CHECK_MESSAGE(status.is_ok(), status.get_msg());
    CHECK_EQ(0, node0.get_kvs().get_size());
    CHECK_EQ(1, node1.get_kvs().get_size());

    //Migration should be done
    CHECK_EQ(cluster::SlotState::c_NORMAL, node0.get_cluster_state().slots[0].state);
    CHECK_EQ(cluster::SlotState::c_NORMAL, node1.get_cluster_state().slots[0].state);


    std::cout << "All tests completed" << std::endl;
    //Stop nodes
    node0.stop();
    if (thread0.joinable()) thread0.join();
    node1.stop();
    if (thread1.joinable()) thread1.join();
    node2.stop();
    if (thread2.joinable()) thread2.join();
}
