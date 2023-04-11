#include <cmath>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string.h>
#include <algorithm>

#include "Cluster.hpp"
#include "ProtocolHandler.hpp"
#include "net/Socket.hpp"

namespace node::cluster {

    ClusterNodeGossipData convert_node_to_network_order(ClusterNodeGossipData& node) {
        ClusterNodeGossipData converted_node = node;
        converted_node.cluster_port = htons(node.cluster_port);
        converted_node.client_port = htons(node.client_port);
        converted_node.num_slots_served = htons(node.num_slots_served);
        return std::move(converted_node);
    }

    ClusterNodeGossipData convert_node_to_host_order(ClusterNodeGossipData& node) {
        ClusterNodeGossipData converted_node = node;
        converted_node.cluster_port = ntohs(node.cluster_port);
        converted_node.client_port = ntohs(node.client_port);
        converted_node.num_slots_served = ntohs(node.num_slots_served);
        return std::move(converted_node);
    }

    void send_ping(net::Connection& link, ClusterState& state) {
        auto required_nodes = static_cast<uint16_t>(ceil(state.size / 10.0));
        ClusterGossipMsg msg;

        std::mt19937 random_engine(std::random_device{}());
        std::uniform_int_distribution<uint16_t> dist(0, state.size - 1); //Important: [a, b] both borders inclusive
        msg.data.emplace_back(convert_node_to_network_order(state.myself));

        for (int i = 0; i < required_nodes; i++) {
            uint16_t random_index = dist(random_engine);
            ClusterNode& rand_node = std::next(state.nodes.begin(), random_index)->second;
            ClusterNodeGossipData converted_node = convert_node_to_network_order(rand_node);
            msg.data.emplace_back(converted_node);
        }

        protocol::send_instruction(link,
            protocol::Command{},
            protocol::Instruction::c_CLUSTER_PING,
            reinterpret_cast<char*>(msg.data.data()),
            (required_nodes + 1) * sizeof(ClusterNodeGossipData));
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


    void update_served_slots_by_node(ClusterState& state, ClusterNode& node) {
        node.num_slots_served = node.served_slots.count();
        for (int i = 0; i < CLUSTER_AMOUNT_OF_SLOTS; i++) {
            if (node.served_slots.test(i)) {
                state.slots[i].served_by = &node;
            }
        }
    }


    void handle_ping(net::Connection& link, ClusterState& state, uint64_t payload_size) {
        auto sent_nodes = static_cast<uint16_t>(payload_size / sizeof(ClusterNodeGossipData));

        for (int i = 0; i < sent_nodes; i++) {
            ClusterNodeGossipData cur;
            protocol::get_payload(link, reinterpret_cast<char*>(&cur), sizeof(ClusterNodeGossipData));
            std::string name(cur.name.begin());
            update_node(name, state, convert_node_to_host_order(cur));
            update_served_slots_by_node(state, state.nodes[name]);
        }

        state.size = state.nodes.size();
    }


    Status add_node(ClusterState& state, const std::string& name, const std::string& ip, uint16_t cluster_port, uint16_t client_port) {
        net::Socket socket{};
        ClusterNode node{};

        try {
            net::Connection link = socket.connect(ip, cluster_port);
            node.outgoing_link = std::move(link);
        }
        catch (std::runtime_error& e) {
            return Status::new_error("Could not connect to node: " + std::string(e.what()));
        }

        node.cluster_port = cluster_port;
        node.client_port = client_port;
        strncpy(node.name.data(), name.data(), std::min(CLUSTER_NAME_LEN, static_cast<uint16_t>(name.size())));
        strncpy(node.ip.data(), ip.data(), std::min(CLUSTER_IP_LEN, static_cast<uint16_t>(ip.size())));

        state.nodes[std::string(node.name.begin())] = std::move(node);
        state.size = state.nodes.size();
        return Status::new_ok();
    }


    uint16_t get_key_hash(const std::string& key) {
        // No hash tag
        if (key.find('{') == std::string::npos || key.find('}') == std::string::npos) {
            return std::hash<std::string>{}(key);
        }

        // Hash tag
        auto start = key.find('{') + 1;
        auto end = key.find('}', start);
        if (start < end && end != std::string::npos) {
            return std::hash<std::string>{}(key.substr(start, end - start));
        }
        return std::hash<std::string>{}(key);
    }

    bool check_key_slot_served_and_send_moved(const std::string& key, net::Connection& connection, cluster::ClusterState& state) {
        uint16_t slot = get_key_hash(key) % CLUSTER_AMOUNT_OF_SLOTS;
        return check_slot_served_and_send_moved(slot, connection, state);
    }

    bool check_slot_served_and_send_moved(uint16_t slot, net::Connection& connection, cluster::ClusterState& state) {
        if (state.myself.served_slots.test(slot)) {
            return true;
        }

        if (slot < 0 || slot >= state.slots.size() || state.slots[slot].served_by == nullptr) {
            protocol::send_instruction(connection, Status::new_error("Slot not served by any node"));
        }
        else {
            ClusterNode& serving_node = *state.slots[slot].served_by;
            protocol::send_instruction(
                connection,
                protocol::Command{std::string(serving_node.ip.data()), std::to_string(serving_node.client_port)},
                protocol::Instruction::c_MOVE
            );
        }
        return false;
    }
}