#pragma once

#include <bitset>
#include <vector>
#include <unordered_map>

#include "../net/Connection.hpp"
#include "../utils/Status.hpp"

//This is required to avoid circular import
namespace node::protocol {
    using Command = std::vector<std::string>;
}


template<typename T>
using observer_ptr = T*;

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

    enum class SlotState: uint8_t {
        c_NORMAL = 0,
        c_MIGRATING = 1,
        c_IMPORTING = 2,
        enum_size = 3
    };

    struct Slot {
        observer_ptr<ClusterNode> served_by = nullptr;
        uint64_t amount_of_keys = 0;
        SlotState state = SlotState::c_NORMAL;
        observer_ptr<ClusterNode> migration_partner = nullptr;
    };

    struct SlotGossipData {
        uint16_t slot_number;
        uint64_t amount_of_keys;
        SlotState state;
        std::array<char, CLUSTER_NAME_LEN> migration_partner_name;
        std::array<char, CLUSTER_NAME_LEN> served_by_name;
    };

    struct ClusterState {
        std::unordered_map<std::string, ClusterNode> nodes;
        uint16_t size;
        std::vector<Slot> slots;
        ClusterNode myself;
    };

    struct ClusterGossipMsg {
        std::vector<ClusterNodeGossipData> nodes;
        std::vector<SlotGossipData> slots;
    };

    void send_ping(observer_ptr<net::Connection> link, ClusterState& state);
    void send_ping(ClusterState& state);

    void handle_ping(net::Connection& link, ClusterState& state, const protocol::Command& comand);

    Status add_node(ClusterState& state, const std::string& name, const std::string& ip, uint16_t cluster_port, uint16_t client_port);

    bool check_key_slot_served_and_send_moved(const std::string& key, net::Connection& connection, cluster::ClusterState& state);

    bool check_slot_served_and_send_moved(uint16_t slot, net::Connection& connection, cluster::ClusterState& state);

    uint16_t get_key_hash(const std::string& key);

}
