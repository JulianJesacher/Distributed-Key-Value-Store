#include "Client.hpp"

#include <random>
#include <stdexcept>

using namespace node::protocol;

namespace client {

    bool Client::connect_to_node(const std::string& ip, int port) {
        net::Socket socket{};
        std::string ip_port = ip + ":" + std::to_string(port);
        try {
            nodes_connections_.emplace(ip_port, socket.connect(ip, port));
        }
        catch (std::runtime_error& e) {
            return false;
        }
        return true;
    }

    void Client::disconnect_all() {
        nodes_connections_.clear();
    }

    ResponseData get_response(net::Connection& connection) {
        MetaData received_meta_data = get_metadata(connection);
        Command received_cmd;
        ByteArray received_payload;

        if (received_meta_data.command_size > 0) {
            received_cmd = get_command(connection, received_meta_data.argc, received_meta_data.command_size);
        }
        if (received_meta_data.payload_size > 0) {
            received_payload = get_payload(connection, received_meta_data.payload_size);
        }
        return std::make_tuple(std::move(received_meta_data), std::move(received_cmd), std::move(received_payload));
    }

    bool Client::handle_move(Command& received_cmd, uint16_t slot) {
        std::string ip = received_cmd[to_integral(CommandFieldsMove::c_OTHER_IP)];
        int port = std::stoi(received_cmd[to_integral(CommandFieldsMove::c_OTHER_CLIENT_PORT)]);
        std::string ip_port = ip + ":" + std::to_string(port);

        if (!nodes_connections_.contains(ip_port)) {
            if (!connect_to_node(ip, port)) {
                return false;
            }
            slots_nodes_[slot] = ip_port;
        }
        return true;
    }

    Status Client::put_value(const std::string& key, const ByteArray& value, int offset) {
        return put_value(key, value.data(), value.size(), offset);
    }

    Status Client::put_value(const std::string& key, const std::string& value, int offset) {
        return put_value(key, value.data(), value.size(), offset);
    }

    Status Client::put_value(const std::string& key, const char* value, uint64_t size, int offset) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;
        observer_ptr<net::Connection> link = nullptr;

        //Slot handled by unknown node, but another random node available
        if (slots_nodes_[slot_number].empty() && !nodes_connections_.empty()) {
            std::mt19937 random_engine(std::random_device{}());
            std::uniform_int_distribution<uint16_t> dist(0, nodes_connections_.size() - 1); //Important: [a, b] both borders inclusive
            uint16_t random_index = dist(random_engine);
            link = &std::next(nodes_connections_.begin(), random_index)->second;
        }
        //No node available
        else if (nodes_connections_.empty()) {
            return Status::new_error("Not connected to any node");
        }
        //Slot handled by known node
        else {
            link = &nodes_connections_.at(slots_nodes_[slot_number]);
        }

        Command cmd{ key, std::to_string(size), std::to_string(offset) };
        send_instruction(*link, cmd, Instruction::c_PUT, value, size);

        //handle response
        ResponseData response = get_response(*link);
        MetaData& received_meta_data = std::get<to_integral(ResponseDataFields::c_METADATA)>(response);
        Command& received_cmd = std::get<to_integral(ResponseDataFields::c_COMMAND)>(response);
        ByteArray& received_payload = std::get<to_integral(ResponseDataFields::c_PAYLOAD)>(response);

        if (received_meta_data.instruction == Instruction::c_OK_RESPONSE) {
            return Status::new_ok();
        }
        else if (received_meta_data.instruction == Instruction::c_ERROR_RESPONSE) {
            return Status::new_error(received_payload.to_string());
        }
        else if (received_meta_data.instruction == Instruction::c_MOVE) {
            if (!handle_move(received_cmd, slot_number)) {
                return Status::new_error("Could not connect to new node");
            }
            return put_value(key, value, size, offset);
        }
        else {
            return Status::new_unknown_response("Unknown response");
        }
    }

}  // namespace client