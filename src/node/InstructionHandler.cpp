#include "InstructionHandler.hpp"
#include "ProtocolHandler.hpp"

using PutFields = node::protocol::CommandFieldsPut;
using GetFields = node::protocol::CommandFieldsGet;
using EraseFields = node::protocol::CommandFieldsErase;
using MeetFields = node::protocol::CommandFieldsMeet;
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
        default:
            return Status::new_invalid_argument("Unknown instruction");
        }
        return Status::new_ok();
    }

    void handle_put(net::Connection& connection, const protocol::MetaData& meta_data,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs) {
        Status argc_state = check_argc(command, Instruction::c_PUT);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, {}, Instruction::c_ERROR_RESPONSE, argc_state.get_msg());
            return;
        }

        uint64_t total_payload_size = meta_data.payload_size;
        uint64_t cur_payload_size = std::stoull(command[to_integral(PutFields::c_CUR_PAYLOAD_SIZE)]);
        uint64_t offset = std::stoull(command[to_integral(PutFields::c_OFFSET)]);
        const std::string& key = command[to_integral(PutFields::c_KEY)];

        //key doesn't exist
        if (!kvs.contains_key(key)) {
            ByteArray payload = ByteArray::new_allocated_byte_array(total_payload_size);
            protocol::get_payload(connection, payload.data(), cur_payload_size);

            Status state = kvs.put(key, payload);
            protocol::send_instruction(connection, state);
            return;
        }

        ByteArray existing{};
        Status state = kvs.get(key, existing);
        existing.resize(total_payload_size);

        //Store the new payload in the existing payload
        protocol::get_payload(connection, existing.data() + offset, cur_payload_size);

        protocol::send_instruction(connection, state);
    }

    void handle_get(net::Connection& connection, const protocol::command& command, key_value_store::IKeyValueStore& kvs) {
        Status argc_state = check_argc(command, Instruction::c_GET);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, argc_state);
            return;
        }

        const std::string& key = command[to_integral(GetFields::c_KEY)];
        uint64_t size = std::stoull(command[to_integral(GetFields::c_SIZE)]);
        uint64_t offset = std::stoull(command[to_integral(GetFields::c_OFFSET)]);

        ByteArray value{};
        Status state = kvs.get(key, value);
        if (!state.is_ok()) {
            protocol::send_instruction(connection, state);
            return;
        }

        protocol::command response_command{std::to_string(value.size()), std::to_string(offset)};
        protocol::send_instruction(connection, response_command,
            Instruction::c_GET_RESPONSE, value.data() + offset, size);
    }

    void handle_erase(net::Connection& connection, const protocol::command& command, key_value_store::IKeyValueStore& kvs) {
        Status argc_state = check_argc(command, Instruction::c_ERASE);
        if (!argc_state.is_ok()) {
            protocol::send_instruction(connection, argc_state);
            return;
        }

        const std::string& key = command[to_integral(EraseFields::c_KEY)];
        Status state = kvs.erase(key);
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
}