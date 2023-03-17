#include "InstructionHandler.hpp"
#include "ProtocolHandler.hpp"

using PutFields = node::protocol::CommandFieldsPut;
using GetFields = node::protocol::CommandFieldsGet;
using EraseFields = node::protocol::CommandFieldsErase;
using Instruction = node::protocol::Instruction;

namespace node::instruction_handler {

    void handle_put(net::Connection& connection, const protocol::MetaData& meta_data,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs) {
        uint64_t total_payload_size = meta_data.payload_size;
        uint64_t cur_payload_size = std::stoull(command[to_integral(PutFields::c_CUR_PAYLOAD_SIZE)]);
        uint64_t offset = std::stoull(command[to_integral(PutFields::c_OFFSET)]);
        const std::string& key = command[to_integral(PutFields::c_KEY)];

        //key doesn't exist
        if (!kvs.contains_key(key)) {
            ByteArray payload = ByteArray::new_allocated_byte_array(total_payload_size);
            protocol::get_payload(connection, payload.data(), cur_payload_size);
            Status state = kvs.put(key, payload);
            if (!state.is_ok()) {
                //TODO: handle error
            }
            return;
        }

        ByteArray existing{};
        kvs.get(key, existing);
        existing.resize(total_payload_size);
        protocol::get_payload(connection, existing.data() + offset, cur_payload_size);
    }

    void handle_get(net::Connection& connection, const protocol::MetaData& meta_data,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs) {
        const std::string& key = command[to_integral(GetFields::c_KEY)];
        uint64_t size = std::stoull(command[to_integral(GetFields::c_SIZE)]);
        uint64_t offset = std::stoull(command[to_integral(GetFields::c_OFFSET)]);

        ByteArray value{};
        Status state = kvs.get(key, value);
        if (!state.is_ok()) {
            //TODO: handle error
        }

        protocol::command response_command{std::to_string(value.size()), std::to_string(offset)};
        protocol::send_response(connection, response_command,
            Instruction::c_GET_RESPONSE, value.data() + offset, size);
    }

    void handle_erase(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs) {
        const std::string& key = command[to_integral(EraseFields::c_KEY)];
        Status state = kvs.erase(key);

        if(state.is_ok()){
            protocol::send_response(connection, {}, Instruction::c_OK_RESPONSE);
        } else {
            protocol::send_response(connection, {}, Instruction::c_ERROR_RESPONSE, state.get_msg());
        }
    }
}