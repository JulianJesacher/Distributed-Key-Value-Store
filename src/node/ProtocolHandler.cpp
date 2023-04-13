#include <stdexcept>
#include <cstring>
#include <endian.h>

#include "ProtocolHandler.hpp"

namespace node::protocol {

    MetaData get_metadata(net::Connection& connection) {
        MetaData meta_data;
        ssize_t received = connection.receive(reinterpret_cast<char*>(&meta_data), sizeof(MetaData));
        if (received != sizeof(MetaData)) {
            throw std::runtime_error("Failed to receive metadata");
        }

        //Convert to host byte order
        meta_data.argc = ntohs(meta_data.argc);
        meta_data.command_size = be64toh(meta_data.command_size);
        meta_data.payload_size = be64toh(meta_data.payload_size);

        return meta_data;
    }

    Command get_command(net::Connection& connection, uint16_t argc, uint64_t command_size) {
        if (argc == 0 || command_size == 0) {
            return {};
        }

        char buf[command_size];
        std::span<char> received_data(buf, command_size);
        ssize_t received = connection.receive(received_data);
        if (received != command_size) {
            throw std::runtime_error("Failed to receive command");
        }
        auto it = received_data.begin();

        Command command(argc);
        for (int i = 0; i < argc; ++i) {
            uint64_t size;
            std::memcpy(&size, &(*it), sizeof(uint64_t));
            size = be64toh(size);

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

    void send_instruction(net::Connection& connection, const Command& command, Instruction i, const char* payload, uint64_t payload_size) {
        uint64_t command_size = get_command_size(command);
        MetaData meta_data{};
        meta_data.instruction = i;
        meta_data.argc = htons(static_cast<uint16_t>(command.size()));
        meta_data.command_size = htobe64(command_size);
        meta_data.payload_size = htobe64(payload_size);


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

    void send_instruction(net::Connection& connection, const Command& command, Instruction i, const std::string& payload) {
        send_instruction(connection, command, i, payload.data(), payload.size());
    }

    uint64_t get_command_size(const Command& command) {
        uint64_t size = 0;
        for (const auto& c : command) {
            size += sizeof(uint64_t) + c.size();
        }
        return size;
    }

    void serialize_command(const Command& command, std::span<char> buf) {
        uint64_t offset = 0;
        for (const auto& c : command) {
            uint64_t size = c.size();
            uint64_t converted_size = htobe64(size);
            std::memcpy(buf.data() + offset, &converted_size, sizeof(uint64_t));
            offset += sizeof(uint64_t);

            std::memcpy(buf.data() + offset, c.data(), size);
            offset += size;
        }
    }

    void write_slot_to_buffer(observer_ptr<cluster::ClusterNode> node, std::span<char> data, uint64_t& offset, int slot_number) {
        //Add end slot range
        data.data()[offset++] = '\t';
        std::string end_slot = std::to_string(slot_number - 1);
        std::memcpy(data.data() + offset, end_slot.data(), end_slot.size());
        offset += end_slot.size();

        //Add ip
        data.data()[offset++] = '\t';
        std::string ip = "NULL";
        if (node != nullptr) {
            ip = std::string((*node).ip.data());
        }
        std::memcpy(data.data() + offset, ip.data(), ip.size());
        offset += ip.size();

        //Add port
        if (node != nullptr) {
            data.data()[offset++] = ':';
            std::string port = std::to_string((*node).client_port);
            std::memcpy(data.data() + offset, port.data(), port.size());
            offset += port.size();
        }
    }

    //<Slot-begin>\t<Slot-end>\t<ip:port>\n
    void serialize_slots(const std::vector<cluster::Slot>& slots, net::Connection& connection) {
        uint64_t offset = 0;
        char buf[(5 + 1 + 5 + 1 + 15 + 1 + 5 + 1) * slots.size()];
        std::span<char> data(buf, sizeof(buf));
        observer_ptr<cluster::ClusterNode> prev_node = nullptr;

        for (uint16_t slot_number = 0; slot_number < slots.size(); ++slot_number) {
            const auto& slot = slots[slot_number];
            if (slot_number == 0) {
                data.data()[offset++] = '0';
                prev_node = slot.served_by;
            }
            //The last slot always needs to be written
            if (slot.served_by == prev_node) {
                continue;
            }

            //New slot range begins, finish old one
            write_slot_to_buffer(prev_node, data, offset, slot_number);
            prev_node = slot.served_by;

            //Add start of next slot range
            data.data()[offset++] = '\n';
            std::string begin_slot = std::to_string(slot_number);
            std::memcpy(data.data() + offset, begin_slot.data(), begin_slot.size());
            offset += begin_slot.size();
        }

        //Write last slot range
        write_slot_to_buffer(prev_node, data, offset, slots.size());

        protocol::send_instruction(connection, {}, protocol::Instruction::c_OK_RESPONSE, data.data(), offset);
    }
}