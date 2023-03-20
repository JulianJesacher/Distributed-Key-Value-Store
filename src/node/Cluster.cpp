#include <cmath>
#include <iterator>
#include <random>

#include "Cluster.hpp"
#include "ProtocolHandler.hpp"
#include "net/Socket.hpp"

namespace node::cluster {

    ClusterNodeGossipData convert_node_to_network_order(ClusterNodeGossipData& node) {
        ClusterNodeGossipData converted_node = node;
        converted_node.node_id = htons(node.node_id);
        converted_node.cluster_port = htons(node.cluster_port);
        converted_node.client_port = htons(node.client_port);
        converted_node.num_slots_served = htons(node.num_slots_served);
        return std::move(converted_node);
    }

    ClusterNodeGossipData convert_node_to_host_order(ClusterNodeGossipData& node) {
        ClusterNodeGossipData converted_node = node;
        converted_node.node_id = ntohs(node.node_id);
        converted_node.cluster_port = ntohs(node.cluster_port);
        converted_node.client_port = ntohs(node.client_port);
        converted_node.num_slots_served = ntohs(node.num_slots_served);
        return std::move(converted_node);
    }

    void send_ping(net::Connection& link, ClusterState& state) {
        uint16_t required_nodes = static_cast<uint16_t>(ceil(state.size / 10.0));
        ClusterGossipMsg msg;

        std::mt19937 random_engine(std::random_device{}());
        std::uniform_int_distribution<uint16_t> dist(0, state.size - 1); //Important: [a, b] both borders inclusive

        for (int i = 0; i < required_nodes; i++) {
            uint16_t random_index = dist(random_engine);
            ClusterNode& rand_node = std::next(state.nodes.begin(), random_index)->second;
            ClusterNodeGossipData converted_node = convert_node_to_network_order(rand_node);
            msg.data.emplace_back(converted_node);
        }

        protocol::send_instruction(link,
            protocol::command{},
            protocol::Instruction::c_CLUSTER_PING,
            reinterpret_cast<char*>(msg.data.data()),
            required_nodes * sizeof(ClusterNodeGossipData));
    }

    void update_node(const std::string& name, ClusterState& state, const ClusterNodeGossipData& node) {
        ClusterNode new_node = static_cast<ClusterNode>(node);

        //Existing connection
        if (state.nodes.contains(name) && state.nodes[name].outgoing_link.is_connected()) {
            new_node.outgoing_link = std::move(state.nodes[name].outgoing_link);
        }
        //New connection required
        else {
            net::Socket socket{};
            new_node.outgoing_link = socket.connect(node.ip.data(), node.cluster_port);
        }
        state.nodes[name] = std::move(new_node);
    }


    void handle_ping(net::Connection& link, ClusterState& state, uint64_t payload_size) {
        uint16_t sent_nodes = static_cast<uint16_t>(payload_size / sizeof(ClusterNode));

        for (int i = 0; i < sent_nodes; i++) {
            ClusterNodeGossipData cur;
            protocol::get_payload(link, reinterpret_cast<char*>(&cur), sizeof(ClusterNodeGossipData));
            std::string name(cur.name.begin());
            update_node(name, state, convert_node_to_host_order(cur));
        }

        state.size = state.nodes.size();
    }
}