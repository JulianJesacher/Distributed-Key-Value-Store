#include <cmath>
#include <iterator>
#include <random>

#include "Cluster.hpp"
#include "ProtocolHandler.hpp"

namespace node::cluster {

    ClusterNode convert_node_to_network_order(ClusterNode& node) {
        ClusterNode converted_node = node;
        converted_node.node_id = htons(node.node_id);
        converted_node.cluster_port = htons(node.cluster_port);
        converted_node.client_port = htons(node.client_port);
        converted_node.num_slots_served = htons(node.num_slots_served);
        return std::move(converted_node);
    }

    ClusterNode convert_node_to_host_order(ClusterNode& node) {
        ClusterNode converted_node = node;
        converted_node.node_id = ntohs(node.node_id);
        converted_node.cluster_port = ntohs(node.cluster_port);
        converted_node.client_port = ntohs(node.client_port);
        converted_node.num_slots_served = ntohs(node.num_slots_served);
        return std::move(converted_node);
    }

    void send_ping(ClusterLink& link, ClusterState& state) {
        uint16_t required_nodes = static_cast<uint16_t>(ceil(state.size / 10.0));
        ClusterGossipMsg msg;

        std::mt19937 random_engine(std::random_device{}());
        std::uniform_int_distribution<uint16_t> dist(0, state.size - 1); //Important: [a, b] both borders inclusive

        for (int i = 0; i < required_nodes; i++) {
            uint16_t random_index = dist(random_engine);
            ClusterNode& rand_node = std::next(state.nodes.begin(), random_index)->second;
            ClusterNode converted_node = convert_node_to_network_order(rand_node);
            msg.data.emplace_back(converted_node);
        }

        protocol::send_instruction(link.connection,
            protocol::command{},
            protocol::Instruction::c_CLUSTER_PING,
            reinterpret_cast<char*>(msg.data.data()),
            required_nodes * sizeof(ClusterNode));
    }


    void handle_ping(ClusterLink& link, ClusterState& state, uint64_t payload_size) {
        uint16_t sent_nodes = static_cast<uint16_t>(payload_size / sizeof(ClusterNode));

        for (int i = 0; i < sent_nodes; i++) {
            ClusterNode cur;
            protocol::get_payload(link.connection, reinterpret_cast<char*>(&cur), sizeof(ClusterNode));
            std::string name(cur.name.begin());
            state.nodes[name] = convert_node_to_host_order(cur);
        }

        state.size = state.nodes.size();
    }
}