#include <cstdint>
#include <vector>

#include "../net/FileDescriptor.hpp"
#include "../net/Connection.hpp"

namespace node {

    namespace protocol {

        // Package structure: metadata | command_1_size | command_1 | command_2_size | command_2 | ... | payload_size | payload
        // commandX_size is of type uint64_t
        enum class Instruction: uint16_t {
            c_PUT = 0,
            c_GET = 1,
            c_REMOVE = 2,
        };

        struct metadata {
            uint16_t argc;
            Instruction instruction;
            uint64_t command_size;
            uint64_t payload_size;
        };

        using command = std::vector<std::string>;

        metadata get_metadata(net::Connection& connection);

        command get_command(net::Connection& connection, uint16_t argc, uint64_t command_size);
    }
}



