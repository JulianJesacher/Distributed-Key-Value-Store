#pragma once

#include <bitset>
#include <vector>
#include <unordered_map>

#include "../net/Connection.hpp"
#include "../utils/Status.hpp"

namespace node::cluster {

    constexpr uint16_t CLUSTER_AMOUNT_OF_SLOTS = 3;
    constexpr uint16_t CLUSTER_NAME_LEN = 40;
    constexpr uint16_t CLUSTER_IP_LEN = 15;

    struct ClusterNodeGossipData;
    struct ClusterNode;

    struct ClusterNodeGossipData {
        std::array<char, CLUSTER_NAME_LEN> name;
        std::array<char, CLUSTER_IP_LEN> ip;
        uint16_t cluster_port;
        uint16_t client_port;
        std::bitset<CLUSTER_AMOUNT_OF_SLOTS> served_slots;
        uint16_t num_slots_served;
    };

    struct ClusterNode: public ClusterNodeGossipData {
        net::Connection outgoing_link;
    };

    struct ClusterState {
        std::unordered_map<std::string, ClusterNode> nodes;
        uint16_t size;
        std::vector<std::reference_wrapper<ClusterNode>> slots;
        ClusterNode myself;
    };

    struct ClusterGossipMsg {
        std::vector<ClusterNodeGossipData> data;
    };

    void send_ping(net::Connection& link, ClusterState& state);

    void handle_ping(net::Connection& link, ClusterState& state, uint64_t payload_size);

    Status add_node(ClusterState& state, const std::string& name, const std::string& ip, uint16_t cluster_port, uint16_t client_port);

    bool check_key_slot_served_and_send_meet(const std::string& key, net::Connection& connection, cluster::ClusterState& state);

}