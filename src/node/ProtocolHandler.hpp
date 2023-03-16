#pragma once

#include <cstdint>
#include <vector>

#include "../net/FileDescriptor.hpp"
#include "../net/Connection.hpp"
#include "../utils/ByteArray.hpp"

namespace node {

    namespace protocol {

        template<typename E>
        constexpr auto to_integral(E e) noexcept {
            return static_cast<std::underlying_type_t<E>>(e);
        }

        // Package structure: metadata | command_1_size | command_1 | command_2_size | command_2 | ... | payload_size | payload
        // commandX_size is of type uint64_t
        enum class Instruction: uint16_t {
            c_PUT = 0,
            c_GET = 1,
            c_REMOVE = 2,
        };

        struct MetaData {
            uint16_t argc;
            Instruction instruction;
            uint64_t command_size;
            uint64_t payload_size;
        };

        enum class CommandFieldsPut {
            c_KEY = 0,
            c_CUR_PAYLOAD_SIZE = 1,
            c_OFFSET = 2,
            //No value since that is stored directly in the byte array and not as part of the commands
        };

        enum class CommandFieldsGet {
            c_KEY = 0,
        };

        enum class CommandFieldsRemove {
            c_KEY = 0,
        };

        using command = std::vector<std::string>;

        MetaData get_metadata(net::Connection& connection);

        command get_command(net::Connection& connection, uint16_t argc, uint64_t command_size, bool payload_exists = false);

        ByteArray get_payload(net::Connection& connection, uint64_t payload_size);

        void get_payload(net::Connection& connection, char* dest, uint64_t payload_size);

        void get_payload(net::Connection& connection, char* dest, uint64_t payload_size);
    }
}



