#include <stdexcept>
#include <cstring>

#include "ProtocolHandler.hpp"

namespace node::protocol {

    MetaData get_metadata(net::Connection& connection) {
        MetaData meta_data;
        connection.receive(reinterpret_cast<char*>(&meta_data), sizeof(MetaData));
        return meta_data;
    }

    command get_command(net::Connection& connection, uint16_t argc, uint64_t command_size) {
        if (argc == 0 || command_size == 0) {
            return {};
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

    void send_instruction(net::Connection& connection, const command& command, Instruction i, const char* payload, uint64_t payload_size) {
        uint64_t command_size = get_command_size(command);
        MetaData meta_data{ static_cast<uint16_t>(command.size()), i, command_size, payload_size };

        //Concatenate metadata and command to save one send() syscall
        uint64_t size_without_payload = sizeof(meta_data) + command_size;
        char buf[size_without_payload];
        std::span<char> data(buf, size_without_payload);

        std::memcpy(data.data(), &meta_data, sizeof(meta_data));
        serialize_command(command, data.subspan(sizeof(meta_data)));
        connection.send(data);

        if (payload != nullptr && payload_size > 0) {
            connection.send(payload, payload_size);
        }
    }

    void send_instruction(net::Connection& connection, const  Status& state) {
        if (state.is_ok()) {
            send_instruction(connection, {}, Instruction::c_OK_RESPONSE);
            return;
        }

        const std::string& error_msg = state.get_msg();
        send_instruction(connection, { }, Instruction::c_ERROR_RESPONSE, error_msg);
    }

    void send_instruction(net::Connection& connection, const command& command, Instruction i, const std::string& payload) {
        send_instruction(connection, command, i, payload.data(), payload.size());
    }

    uint64_t get_command_size(const command& command) {
        uint64_t size = 0;
        for (const auto& c : command) {
            size += sizeof(uint64_t) + c.size();
        }
        return size;
    }

    void serialize_command(const command& command, std::span<char> buf) {
        uint64_t offset = 0;
        for (const auto& c : command) {
            uint64_t size = c.size();
            std::memcpy(buf.data() + offset, &size, sizeof(uint64_t));
            offset += sizeof(uint64_t);
            std::memcpy(buf.data() + offset, c.data(), size);
            offset += size;
        }
    }
}
