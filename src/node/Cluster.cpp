#include <cmath>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string.h>
#include <algorithm>
#include <endian.h>

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

    SlotGossipData get_slot_data(Slot& slot, uint16_t slot_number) {
        SlotGossipData slot_data;
        slot_data.amount_of_keys = slot.amount_of_keys;
        slot_data.slot_number = slot_number;
        slot_data.state = slot.state;

        if (slot.migration_partner != nullptr) {
            slot_data.migration_partner_name = slot.migration_partner->name;
        }
        else {
            slot_data.migration_partner_name.fill(0);
        }

        if (slot.served_by != nullptr) {
            slot_data.served_by_name = slot.served_by->name;
        }
        else {
            slot_data.served_by_name.fill(0);
        }

        return slot_data;
    }

    void convert_slot_to_network_order(SlotGossipData& slot) {
        slot.amount_of_keys = htobe64(slot.amount_of_keys);
        slot.slot_number = htobe16(slot.slot_number);
        //TODO: Status?
    }

    void convert_slot_to_host_order(SlotGossipData& slot) {
        slot.amount_of_keys = be64toh(slot.amount_of_keys);
        slot.slot_number = be16toh(slot.slot_number);
    }

    void send_ping(ClusterState& state) {
        if (state.size == 0) {
            return;
        }

        auto required_nodes = static_cast<uint16_t>(ceil(state.size / 10.0));
        for (uint16_t i = 0; i < required_nodes; ++i) {
            std::mt19937 random_engine(std::random_device{}());
            std::uniform_int_distribution<uint16_t> dist(0, state.size - 1); //Important: [a, b] both borders inclusive
            uint16_t random_index = dist(random_engine);
            ClusterNode& rand_node = std::next(state.nodes.begin(), random_index)->second;

            if (rand_node.outgoing_link.is_connected() && rand_node.name != state.myself.name) {
                send_ping(&rand_node.outgoing_link, state);
            }
        }
    }

    void send_ping(observer_ptr<net::Connection> link, ClusterState& state) {
        auto required_nodes = static_cast<uint16_t>(ceil(state.size / 10.0));
        ClusterGossipMsg msg;
        msg.sender = state.myself.name;

        std::mt19937 random_engine(std::random_device{}());
        std::uniform_int_distribution<uint16_t> dist(0, state.size - 1); //Important: [a, b] both borders inclusive
        msg.nodes.emplace_back(convert_node_to_network_order(state.myself));

        for (int i = 0; i < required_nodes; i++) {
            uint16_t random_index = dist(random_engine);
            ClusterNode& rand_node = std::next(state.nodes.begin(), random_index)->second;
            ClusterNodeGossipData converted_node = convert_node_to_network_order(rand_node);
            msg.nodes.emplace_back(converted_node);
        }

        for (uint16_t slot_number = 0; slot_number < CLUSTER_AMOUNT_OF_SLOTS; slot_number++) {
            SlotGossipData slot_data = get_slot_data(state.slots[slot_number], slot_number);
            convert_slot_to_network_order(slot_data);
            msg.slots.emplace_back(std::move(slot_data));
        }

        uint64_t nodes_size_bytes = (required_nodes + 1) * sizeof(ClusterNodeGossipData); //+1 for myself
        uint64_t slots_size_bytes = CLUSTER_AMOUNT_OF_SLOTS * sizeof(SlotGossipData);
        uint64_t total_size_bytes = nodes_size_bytes + slots_size_bytes;

        protocol::send_instruction(*link,
            protocol::Command{ std::to_string(1 + required_nodes), std::to_string(CLUSTER_AMOUNT_OF_SLOTS) },
            protocol::Instruction::c_CLUSTER_PING,
            reinterpret_cast<char*>(msg.nodes.data()),
            nodes_size_bytes
        );
        link->send(reinterpret_cast<char*>(msg.slots.data()), slots_size_bytes);
        link->send(reinterpret_cast<char*>(msg.sender.data()), msg.sender.size());
        //TODO: Clean up
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


    void handle_ping(net::Connection& link, ClusterState& state, const protocol::Command& comand) {
        uint16_t sent_nodes = std::stoi(comand[to_integral(protocol::CommandFieldsPing::c_NODES_AMOUNT)]);
        uint16_t sent_slots = std::stoi(comand[to_integral(protocol::CommandFieldsPing::c_SLOTS_AMOUNT)]);

        for (int i = 0; i < sent_nodes; i++) {
            ClusterNodeGossipData cur;
            protocol::get_payload(link, reinterpret_cast<char*>(&cur), sizeof(ClusterNodeGossipData));
            std::string name(cur.name.begin());
            update_node(name, state, convert_node_to_host_order(cur));
            update_served_slots_by_node(state, state.nodes[name]); //TODO: Remove
        }

        //Receive all slots into a vector
        std::vector<SlotGossipData> received_slots(sent_slots);
        link.receive(reinterpret_cast<char*>(received_slots.data()), sent_slots * sizeof(SlotGossipData));

        //Get sender
        std::array<char, CLUSTER_NAME_LEN> name;
        link.receive(name.data(), CLUSTER_NAME_LEN);
        std::string sender_name(name.data());

        for (uint16_t slot_number = 0; slot_number < sent_slots; slot_number++) {
            //Every node knows best about it's own slots
            if(state.myself.served_slots.test(slot_number)){
                continue;
            }

            convert_slot_to_host_order(received_slots[slot_number]);
            SlotGossipData& slot_data = received_slots[slot_number];

            std::string served_by_name{ slot_data.served_by_name.data() };
            if (served_by_name.size() != 0 && state.nodes.contains(served_by_name)) {
                state.slots[slot_number].served_by = &state.nodes[served_by_name];
            }

            std::string migration_parner_name{ slot_data.migration_partner_name.data() };
            if (migration_parner_name.size() != 0 && state.nodes.contains(migration_parner_name)) {
                state.slots[slot_number].migration_partner = &state.nodes[migration_parner_name];
            }

            //Only the handling node can upate the slot info, because it knows best and the others don't care about this info
            if (sender_name == std::string(state.myself.name.data())) {
                state.slots[slot_number].amount_of_keys = slot_data.amount_of_keys;
                state.slots[slot_number].state = slot_data.state;
            }
        }

        state.size = state.nodes.size();
        state.part_of_cluster = true;
    }


    Status add_node(ClusterState& state, const std::string& name, const std::string& ip, uint16_t cluster_port, uint16_t client_port) {
        net::Socket socket{};
        ClusterNode node{};

        if (state.nodes.contains(name)) {
            return Status::new_error("Node with name " + name + " already in cluster");
        }

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
                protocol::Command{ std::string(serving_node.ip.data()), std::to_string(serving_node.client_port) },
                protocol::Instruction::c_MOVE
            );
        }
        return false;
    }
}