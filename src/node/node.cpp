#include "node.hpp"
#include "InstructionHandler.hpp"
#include "../KVS/IKeyValueStore.hpp"
#include "../KVS/InMemoryKVS.hpp"
#include "../net/Connection.hpp"
#include "../net/Socket.hpp"

using MetaData = node::protocol::MetaData;
using command = node::protocol::command;
using Instruction = node::protocol::Instruction;

namespace node {
    void Node::execute_instruction(net::Connection& connection, const MetaData& meta_data, const command& command) {
        switch (meta_data.instruction) {
        case Instruction::c_PUT:
            instruction_handler::handle_put(connection, meta_data, command, get_kvs(), get_cluster_state());
            break;
        case Instruction::c_GET:
            instruction_handler::handle_get(connection, command, get_kvs(), get_cluster_state());
            break;
        case Instruction::c_ERASE:
            instruction_handler::handle_erase(connection, command, get_kvs(), get_cluster_state());
            break;
        case Instruction::c_MEET:
            instruction_handler::handle_meet(connection, command, get_cluster_state());
            break;
        case Instruction::c_MIGRATE_SLOT:
            instruction_handler::handle_migrate_slot(connection, command, get_cluster_state());
            break;
        case Instruction::c_IMPORT_SLOT:
            instruction_handler::handle_import_slot(connection, command, get_cluster_state());
            break;
        default:
            protocol::send_instruction(connection, Status::new_not_supported("Unknown instruction"));
            break;
        }
    }

    void Node::handle_connection(net::Connection& connection) {
        MetaData meta_data = node::protocol::get_metadata(connection);
        command command = node::protocol::get_command(connection, meta_data.argc, meta_data.command_size);

        execute_instruction(connection, meta_data, command);
    }
}
