#include "InstructionHandler.hpp"
#include "ProtocolHandler.hpp"

using PutFields = node::protocol::CommandFieldsPut;

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
}