#pragma once

#include <cstdint>
#include <vector>

#include "../net/FileDescriptor.hpp"
#include "../net/Connection.hpp"
#include "../utils/ByteArray.hpp"
#include "../utils/Status.hpp"
#include "../node/Cluster.hpp"

namespace node {

    namespace protocol {

        template<typename E>
        constexpr auto to_integral(E e) noexcept {
            return static_cast<std::underlying_type_t<E>>(e);
        }

        // Package structure: metadata | command_1_size | command_1 | command_2_size | command_2 | ... | payload_size | payload
        // commandX_size is of type uint64_t
        enum class Instruction: uint8_t {
            c_PUT = 0,
            c_GET = 1,
            c_ERASE = 2,
            c_GET_RESPONSE = 3,
            c_OK_RESPONSE = 4,
            c_ERROR_RESPONSE = 5,
            c_CLUSTER_PING = 6,
            c_MEET = 7,
            c_MOVE = 8,
            c_IMPORT_SLOT = 9,
            c_MIGRATE_SLOT = 10,
            c_ASK = 11,
            c_NO_ASKING_ERROR = 12,
            c_CLUSTER_MIGRATION_FINISHED = 13,
            c_GET_SLOTS = 14,
            enum_size = 15
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
            enum_size = 3
        };

        enum class CommandFieldsGet {
            c_KEY = 0,
            c_SIZE = 1,
            c_OFFSET = 2,
            c_ASKING = 3,
            enum_size = 4
        };

        enum class CommandFieldsGetResponse {
            c_SIZE = 0,
            c_OFFSET = 1,
            enum_size = 2
        };

        enum class CommandFieldsErase {
            c_KEY = 0,
            enum_size = 1
        };

        enum class CommandFieldsMeet {
            c_IP = 0,
            c_CLIENT_PORT = 1,
            c_CLUSTER_PORT = 2,
            c_NAME = 3,
            enum_size = 4
        };

        enum class CommandFieldsMove {
            c_OTHER_IP = 0,
            c_OTHER_CLIENT_PORT = 1,
            enum_size = 2
        };

        enum class CommandFieldsMigrate {
            c_SLOT = 0,
            c_OTHER_IP = 1,
            c_OTHER_CLIENT_PORT = 2,
            enum_size = 3
        };

        enum class CommandFieldsImport {
            c_SLOT = 0,
            c_OTHER_IP = 1,
            c_OTHER_CLIENT_PORT = 2,
            enum_size = 3
        };

        enum class CommandFieldsNoAskingError {
            c_OTHER_IP = 0,
            c_OTHER_CLIENT_PORT = 1,
            enum_size = 2
        };

        enum class CommandFieldsMigrationFinished {
            c_SLOT = 0,
            enum_size = 1
        };

        using CommandFieldsAsk = CommandFieldsMove;

        using Command = std::vector<std::string>;

        using ResponseData = std::tuple<MetaData, Command, ByteArray>;

        enum class ResponseDataFields {
            c_METADATA = 0,
            c_COMMAND = 1,
            c_PAYLOAD = 2,
            enum_size = 3
        };

        MetaData get_metadata(net::Connection& connection);

        Command get_command(net::Connection& connection, uint16_t argc, uint64_t command_size);

        ByteArray get_payload(net::Connection& connection, uint64_t payload_size);

        void get_payload(net::Connection& connection, char* dest, uint64_t payload_size);

        void get_payload(net::Connection& connection, char* dest, uint64_t payload_size);

        void send_instruction(net::Connection& connection, const Command& command, Instruction i,
            const char* payload = nullptr, uint64_t payload_size = 0);

        void send_instruction(net::Connection& connection, const  Status& state);

        void send_instruction(net::Connection& connection, const Command& command, Instruction i, const std::string& payload);

        uint64_t get_command_size(const Command& command);

        void serialize_command(const Command& command, std::span<char> buf);

        void serialize_slots(const std::vector<cluster::Slot>& slots, net::Connection& connection);
    }
}
