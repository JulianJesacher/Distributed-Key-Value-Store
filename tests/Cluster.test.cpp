#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <chrono>

#include "node/Cluster.hpp"
#include "net/Socket.hpp"
#include "node/ProtocolHandler.hpp"

using namespace  std::chrono_literals;
using namespace node::cluster;

void compare_clusterNodes(const ClusterNode& lhs, const ClusterNode& rhs) {
    CHECK_EQ(lhs.node_id, rhs.node_id);
    CHECK_EQ(lhs.name, rhs.name);
    CHECK_EQ(lhs.ip, rhs.ip);
    CHECK_EQ(lhs.cluster_port, rhs.cluster_port);
    CHECK_EQ(lhs.client_port, rhs.client_port);
    CHECK_EQ(lhs.served_slots, rhs.served_slots);
    CHECK_EQ(lhs.num_slots_served, rhs.num_slots_served);
}

TEST_CASE("Test Gossip Ping") {

    ClusterState state_receiver, state_sender;
    ClusterNode node1{ 1, "node1", "127.0.0.1", 1234, 1235, std::bitset<c_AMOUNT_OF_SLOTS>{}, 0 };
    state_sender.nodes["node1"] = node1;

    state_sender.size = 1;
    state_receiver.size = 0;

    int port{ 3000 };
    net::Socket sender_socket{}, receiver_socket{};

    auto send_ping = [&]() {
        net::Connection connection = sender_socket.connect(port);
        ClusterLink link{ connection, ClusterNode{}, false };
        node::cluster::send_ping(link, state_sender);
    };

    auto handle_ping = [&]() {
        receiver_socket.listen(port);
        net::Connection connection = receiver_socket.accept();
        ClusterLink link{ connection, ClusterNode{}, true };

        //Receive metadata, because that is not handled by the handle_ping function
        node::protocol::get_metadata(link.connection);

        node::cluster::handle_ping(link, state_receiver, sizeof(ClusterNode));
    };

    SUBCASE("Insert new") {
        CHECK_EQ(state_receiver.nodes.size(), 0);

        auto received = std::async(handle_ping);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_ping);

        sent.get();
        received.get();

        CHECK_EQ(state_receiver.nodes.size(), 1);
        compare_clusterNodes(state_receiver.nodes["node1"], node1);
    }

    SUBCASE("Overwrite existing") {
        state_receiver.nodes["node1"] = node1;
        node1.num_slots_served = 100;
        state_sender.nodes["node1"] = node1;
        CHECK_EQ(state_receiver.nodes.size(), 1);

        auto received = std::async(handle_ping);
        std::this_thread::sleep_for(100ms);
        auto sent = std::async(send_ping);

        sent.get();
        received.get();

        CHECK_EQ(state_receiver.nodes.size(), 1);
        compare_clusterNodes(state_receiver.nodes["node1"], node1);
    }
}