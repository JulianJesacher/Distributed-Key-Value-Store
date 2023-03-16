#include "node.hpp"

using namespace node::protocol;

namespace node {
    void execute_instruction(net::Connection& connection, const MetaData& metadata, const command& command) {
        switch (metadata.instruction) {
        case Instruction::c_PUT:
            break;
        case Instruction::c_GET:
            break;
        case Instruction::c_REMOVE:
            break;
        default:
        }
    }

    void handle_connection(net::Connection& connection) {
        MetaData meta_data = get_metadata(connection);
        command command = get_command(connection, meta_data.argc, meta_data.command_size, meta_data.payload_size > 0);

        execute_instruction(connection, meta_data, command);
    }
}
