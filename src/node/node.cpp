#include "node.hpp"
#include "InstructionHandler.hpp"
#include "../KVS/IKeyValueStore.hpp"
#include "../KVS/InMemoryKVS.hpp"
#include "../net/Connection.hpp"
#include "../net/Socket.hpp"

using namespace node::protocol;

namespace node {
    void Node::execute_instruction(net::Connection& connection, const MetaData& metadata, const command& command) {
        switch (metadata.instruction) {
        case Instruction::c_PUT:
            instruction_handler::handle_put(connection, metadata, command, get_kvs());
            break;
        case Instruction::c_GET:
            instruction_handler::handle_get(connection, metadata, command, get_kvs());
            break;
        case Instruction::c_ERASE:
            instruction_handler::handle_erase(connection, metadata, command, get_kvs());
            break;
        default:
            protocol::send_instruction(connection, Status::new_not_supported("Unknown instruction"));
            break;
        }
    }

    void Node::handle_connection(net::Connection& connection) {
        MetaData meta_data = get_metadata(connection);
        command command = get_command(connection, meta_data.argc, meta_data.command_size);

        execute_instruction(connection, meta_data, command);
    }
}
