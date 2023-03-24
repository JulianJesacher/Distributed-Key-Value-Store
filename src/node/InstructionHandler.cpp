#include <cstring>

#include "InstructionHandler.hpp"
#include "ProtocolHandler.hpp"
#include "Cluster.hpp"

using PutFields = node::protocol::CommandFieldsPut;
using GetFields = node::protocol::CommandFieldsGet;
using EraseFields = node::protocol::CommandFieldsErase;
using MeetFields = node::protocol::CommandFieldsMeet;
using MigrateFields = node::protocol::CommandFieldsMigrate;
using ImportFields = node::protocol::CommandFieldsImport;
using Instruction = node::protocol::Instruction;

namespace node::instruction_handler {

    Status check_argc(const protocol::command& command, protocol::Instruction instruction) {
        switch (instruction) {
        case Instruction::c_PUT:
            if (command.size() != to_integral(PutFields::enum_size)) {
                return Status::new_invalid_argument("Wrong number of arguments for PUT");
            }
            break;
        case Instruction::c_GET:
            if (command.size() != to_integral(GetFields::enum_size)) {
                return Status::new_invalid_argument("Wrong number of arguments for GET");
            }
            break;
        case Instruction::c_ERASE:
            if (command.size() != to_integral(EraseFields::enum_size)) {
                return Status::new_invalid_argument("Wrong number of arguments for ERASE");
            }
            break;
        case Instruction::c_MEET:
            if (command.size() != to_integral(MeetFields::enum_size)) {
                return Status::new_invalid_argument("Wrong number of arguments for MEET");
            }
            break;
        case Instruction::c_MIGRATE_SLOT:
            if (command.size() != to_integral(MigrateFields::enum_size)) {
                return Status::new_invalid_argument("Wrong number of arguments for MIGRATE_SLOT");
            }
            break;
        case Instruction::c_IMPORT_SLOT:
            if (command.size() != to_integral(ImportFields::enum_size)) {
                return Status::new_invalid_argument("Wrong number of arguments for IMPORT_SLOT");
            }
            break;
        default:
            return Status::new_invalid_argument("Unknown instruction");
        }
        return Status::new_ok();
    }

    void send_ask_response(net::Connection& connection, uint16_t slot, cluster::ClusterState& cluster_state) {
        cluster::ClusterNode& migration_partner = *cluster_state.slots[slot].migration_partner;
        protocol::command ask_command{std::string(migration_partner.ip.data()), std::to_string(migration_partner.client_port)};
        protocol::send_instruction(connection, ask_command, Instruction::c_ASK);
    }

    void handle_put(net::Connection& connection, const protocol::MetaData& meta_data,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state) {
        Status argc_state = check_argc(command, Instruction::c_PUT);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, {}, Instruction::c_ERROR_RESPONSE, argc_state.get_msg());
            return;
        }

        uint64_t total_payload_size = meta_data.payload_size;
        uint64_t cur_payload_size = std::stoull(command[to_integral(PutFields::c_CUR_PAYLOAD_SIZE)]);
        uint64_t offset = std::stoull(command[to_integral(PutFields::c_OFFSET)]);
        const std::string& key = command[to_integral(PutFields::c_KEY)];
        uint16_t slot = cluster::get_key_hash(key) % cluster::CLUSTER_AMOUNT_OF_SLOTS;

        if (!cluster::check_key_slot_served_and_send_meet(key, connection, cluster_state)) {
            return;
        }

        //key doesn't exist and slot is not migrating
        if (!kvs.contains_key(key) && cluster_state.slots[slot].state != cluster::SlotState::c_MIGRATING) {
            ByteArray payload = ByteArray::new_allocated_byte_array(total_payload_size);
            protocol::get_payload(connection, payload.data(), cur_payload_size);

            Status state = kvs.put(key, payload);
            protocol::send_instruction(connection, state);
            cluster_state.slots[slot].amount_of_keys += 1;
            return;
        }
        //key doesn't exist and slot is migrating
        else if (cluster_state.slots[slot].state == cluster::SlotState::c_MIGRATING) {
            send_ask_response(connection, slot, cluster_state);
            return;
        }

        //Update stored value
        ByteArray existing{};
        Status state = kvs.get(key, existing);
        existing.resize(total_payload_size);
        //Store the new payload in the existing payload
        protocol::get_payload(connection, existing.data() + offset, cur_payload_size);

        protocol::send_instruction(connection, state);
    }

    void handle_get(net::Connection& connection, const protocol::command& command,
        key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state) {
        Status argc_state = check_argc(command, Instruction::c_GET);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, argc_state);
            return;
        }

        const std::string& key = command[to_integral(GetFields::c_KEY)];
        uint64_t size = std::stoull(command[to_integral(GetFields::c_SIZE)]);
        uint64_t offset = std::stoull(command[to_integral(GetFields::c_OFFSET)]);
        bool asking = command[to_integral(GetFields::c_ASKING)] == "true";
        uint16_t slot = cluster::get_key_hash(key) % cluster::CLUSTER_AMOUNT_OF_SLOTS;

        if (!cluster::check_slot_served_and_send_meet(slot, connection, cluster_state)) {
            return;
        }

        //Check if client has been redirected by asking command if slot is importing
        if (cluster_state.slots[slot].state == cluster::SlotState::c_IMPORTING && !asking) {
            protocol::send_instruction(connection, Status::new_error("The slot is importing, ASKING flag required"));
            return;
        }

        ByteArray value{};
        Status state = kvs.get(key, value);

        //Value not found and slot not migrating -> error
        if (state.is_not_found() && cluster_state.slots[slot].state != cluster::SlotState::c_MIGRATING) {
            protocol::send_instruction(connection, state);
            return;
        }
        //Migration in process, get value from other node
        else if (state.is_not_found() && cluster_state.slots[slot].state == cluster::SlotState::c_MIGRATING) {
            send_ask_response(connection, slot, cluster_state);
            return;
        }

        //Send retrieved value
        protocol::command response_command{std::to_string(value.size()), std::to_string(offset)};
        protocol::send_instruction(connection, response_command,
            Instruction::c_GET_RESPONSE, value.data() + offset, size);
    }

    void handle_erase(net::Connection& connection, const protocol::command& command,
        key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state) {
        Status argc_state = check_argc(command, Instruction::c_ERASE);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, argc_state);
            return;
        }

        const std::string& key = command[to_integral(EraseFields::c_KEY)];
        uint16_t slot = cluster::get_key_hash(key) % cluster::CLUSTER_AMOUNT_OF_SLOTS;
        if (!cluster::check_key_slot_served_and_send_meet(key, connection, cluster_state)) {
            return;
        }

        Status state = kvs.erase(key);

        //Slot migrating and key not found -> respond with ask
        if (state.is_not_found() && cluster_state.slots[slot].state == cluster::SlotState::c_MIGRATING) {
            send_ask_response(connection, slot, cluster_state);
            return;
        }

        //Update slot state
        if (state.is_ok()) {
            uint16_t slot = cluster::get_key_hash(key) % cluster::CLUSTER_AMOUNT_OF_SLOTS;
            cluster_state.slots[slot].amount_of_keys -= 1;

            if (cluster_state.slots[slot].amount_of_keys == 0) {
                cluster_state.slots[slot].state = cluster::SlotState::c_NORMAL;
                cluster_state.slots[slot].migration_partner = nullptr;
                cluster_state.myself.served_slots[slot] = false;
                cluster_state.myself.num_slots_served = cluster_state.myself.served_slots.count();
                //TODO: Send migration partner migration finished
            }
        }

        protocol::send_instruction(connection, state);
    }

    void handle_meet(net::Connection& connection, const protocol::command& command, cluster::ClusterState& cluster_state) {
        Status argc_state = check_argc(command, Instruction::c_MEET);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, argc_state);
            return;
        }

        const std::string& ip = command[to_integral(MeetFields::c_IP)];
        uint16_t port = std::stoul(command[to_integral(MeetFields::c_CLIENT_PORT)]);
        uint16_t cluster_port = std::stoul(command[to_integral(MeetFields::c_CLUSTER_PORT)]);
        const std::string& name = command[to_integral(MeetFields::c_NAME)];

        Status state = cluster::add_node(cluster_state, name, ip, cluster_port, port);
        protocol::send_instruction(connection, state);
    }

    std::optional<cluster::ClusterNode*> get_partner_node_handle_errors(uint16_t slot, const std::string& ip, uint16_t port,
        net::Connection& connection, cluster::ClusterState& cluster_state) {
        //Already in process of migrating
        if (cluster_state.slots[slot].state != cluster::SlotState::c_NORMAL) {
            protocol::send_instruction(connection, Status::new_not_supported("Slot already in process of migrating"));
            return std::nullopt;
        }

        auto partner = std::find_if(cluster_state.nodes.begin(), cluster_state.nodes.end(),
            [&ip, &port](const auto& iterator) {
                const cluster::ClusterNode& node = iterator.second;
                return memcmp(node.ip.data(), ip.data(), ip.size()) == 0 && node.client_port == port;
            });

        //Node not in cluster
        if (partner == cluster_state.nodes.end()) {
            protocol::send_instruction(connection, Status::new_error("Other node not part of the cluster"));
            return std::nullopt;
        }

        return &(partner->second);
    }

    void handle_migrate_slot(net::Connection& connection, const protocol::command& command, cluster::ClusterState& cluster_state) {
        Status argc_state = check_argc(command, Instruction::c_MIGRATE_SLOT);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, argc_state);
            return;
        }

        uint16_t slot = std::stoul(command[to_integral(MigrateFields::c_SLOT)]);
        const std::string& ip = command[to_integral(MigrateFields::c_OTHER_IP)];
        uint16_t port = std::stoul(command[to_integral(MigrateFields::c_OTHER_CLIENT_PORT)]);

        //Not handled by this node
        if (!cluster::check_slot_served_and_send_meet(slot, connection, cluster_state)) {
            protocol::send_instruction(connection, Status::new_error("The slot is not handled by this node"));
            return;
        }

        auto partner = get_partner_node_handle_errors(slot, ip, port, connection, cluster_state);
        //Error occurred or no keys to migrate
        if (!partner.has_value()) {
            return;
        }

        if (cluster_state.slots[slot].amount_of_keys != 0) {
            cluster_state.slots[slot].migration_partner = partner.value();
            cluster_state.slots[slot].state = cluster::SlotState::c_MIGRATING;
        }

        protocol::send_instruction(connection, Status::new_ok());
    }

    void handle_import_slot(net::Connection& connection, const protocol::command& command, cluster::ClusterState& cluster_state) {
        Status argc_state = check_argc(command, Instruction::c_IMPORT_SLOT);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, argc_state);
            return;
        }

        uint16_t slot = std::stoul(command[to_integral(ImportFields::c_SLOT)]);
        const std::string& ip = command[to_integral(ImportFields::c_OTHER_IP)];
        uint16_t port = std::stoul(command[to_integral(ImportFields::c_OTHER_CLIENT_PORT)]);

        auto partner = get_partner_node_handle_errors(slot, ip, port, connection, cluster_state);
        //Error occurred
        if (!partner.has_value()) {
            return;
        }

        cluster_state.slots[slot].migration_partner = partner.value();
        cluster_state.slots[slot].state = cluster::SlotState::c_IMPORTING;
        cluster_state.myself.served_slots[slot] = true;
        cluster_state.myself.num_slots_served = cluster_state.myself.served_slots.count();
        protocol::send_instruction(connection, Status::new_ok());
    }
}