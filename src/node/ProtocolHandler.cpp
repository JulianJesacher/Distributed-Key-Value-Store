#include <stdexcept>

#include "ProtocolHandler.hpp"

namespace node::protocol {

    metadata get_metadata(net::Connection& connection) {
        metadata m_data;
        connection.receive(reinterpret_cast<char*>(&m_data), sizeof(metadata));
        return m_data;
    }

    command get_command(net::Connection& connection, uint16_t argc, uint64_t command_size) {
        char buf[command_size];
        std::span<char> received_data(buf, command_size);
        connection.receive(received_data);
        auto it = received_data.begin();

        command command(argc);
        for(int i=0; i<argc; ++i){
            auto size = static_cast<uint64_t>(*it);
            it += sizeof(uint64_t);
            command[i] = std::string(it, it+size);
            it += size;
        }
        return command;
    }
}