#include <stdexcept>

#include "ProtocolHandler.hpp"

namespace node::protocol {

    MetaData get_metadata(net::Connection& connection) {
        MetaData meta_data;
        connection.receive(reinterpret_cast<char*>(&meta_data), sizeof(MetaData));
        return meta_data;
    }

    command get_command(net::Connection& connection, uint16_t argc, uint64_t command_size, bool payload_exists) {
        //The payload is not added as a command, since it is directly stored in the respective ByteArray
        if (payload_exists) {
            --argc;
        }

        char buf[command_size];
        std::span<char> received_data(buf, command_size);
        connection.receive(received_data);
        auto it = received_data.begin();

        command command(argc);
        for (int i = 0; i < argc; ++i) {
            auto size = static_cast<uint64_t>(*it);
            it += sizeof(uint64_t);
            command[i] = std::string(it, it + size);
            it += size;
        }
        return command;
    }

    ByteArray get_payload(net::Connection& connection, uint64_t payload_size) {
        ByteArray payload = ByteArray::new_allocated_byte_array(payload_size);
        connection.receive(payload.data(), payload_size);
        return std::move(payload);
    }

    void get_payload(net::Connection& connection, char* dest, uint64_t payload_size) {
        connection.receive(dest, payload_size);
    }

}
