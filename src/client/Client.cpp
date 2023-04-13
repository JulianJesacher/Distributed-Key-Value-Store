#include "Client.hpp"

#include <random>
#include <stdexcept>

using namespace node::protocol;

namespace client {

    std::string get_ip_port(const std::string& ip, int port) {
        return ip + ":" + std::to_string(port);
    }
    std::string get_ip_port(const std::string& ip, const std::string& port) {
        return ip + ":" + port;
    }

    bool Client::connect_to_node(const std::string& ip, int port) {
        net::Socket socket{};
        std::string ip_port = get_ip_port(ip, port);
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
        std::string ip_port = get_ip_port(ip, port);

        if (!nodes_connections_.contains(ip_port)) {
            if (!connect_to_node(ip, port)) {
                return false;
            }
            slots_nodes_[slot] = ip_port;
        }
        return true;
    }

    observer_ptr<net::Connection> Client::get_node_connection_by_slot(uint16_t slot_number) {
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
            return nullptr;
        }
        //Slot handled by known node
        else {
            link = &nodes_connections_.at(slots_nodes_[slot_number]);
        }
        return link;
    }

    Status Client::put_value(const std::string& key, const ByteArray& value, int offset) {
        return put_value(key, value.data(), value.size(), offset);
    }

    Status Client::put_value(const std::string& key, const std::string& value, int offset) {
        return put_value(key, value.data(), value.size(), offset);
    }

    Status Client::put_value(observer_ptr<net::Connection> link, const std::string& key, const char* value, uint64_t size, int offset) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;
        //No node available
        if (link == nullptr) {
            return Status::new_error("Not connected to any node");
        }

        Command cmd{ key, std::to_string(size), std::to_string(offset) };
        send_instruction(*link, cmd, Instruction::c_PUT, value, size);

        //handle response
        ResponseData response = get_response(*link);
        MetaData& received_meta_data = std::get<to_integral(ResponseDataFields::c_METADATA)>(response);
        Command& received_cmd = std::get<to_integral(ResponseDataFields::c_COMMAND)>(response);
        ByteArray& received_payload = std::get<to_integral(ResponseDataFields::c_PAYLOAD)>(response);


        switch (received_meta_data.instruction) {
        case Instruction::c_OK_RESPONSE:
        {
            return Status::new_ok();
        }

        case Instruction::c_ERROR_RESPONSE:
        {
            return Status::new_error(received_payload.to_string());
        }

        case Instruction::c_MOVE:
        {
            if (!handle_move(received_cmd, slot_number)) {
                return Status::new_error("Could not connect to new node");
            }
            return put_value(key, value, size, offset);
        }

        case Instruction::c_ASK:
        {
            if (!handle_ask(received_cmd)) {
                return Status::new_error("Could not connect to new node");
            }
            std::string other_ip = received_cmd[to_integral(CommandFieldsAsk::c_OTHER_IP)];
            std::string other_port = received_cmd[to_integral(CommandFieldsAsk::c_OTHER_CLIENT_PORT)];
            std::string ip_port = get_ip_port(other_ip, other_port);
            observer_ptr<net::Connection> new_link = &nodes_connections_[ip_port];
            return put_value(new_link, key, value, size, offset);
        }

        default:
        {
            return Status::new_unknown_response("Unknown response");
        }
        }
    }

    Status Client::put_value(const std::string& key, const char* value, uint64_t size, int offset) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;
        observer_ptr<net::Connection> link = get_node_connection_by_slot(slot_number);
        return put_value(link, key, value, size, offset);
    }

    bool Client::handle_ask(Command& received_cmd) {
        std::string ip = received_cmd[to_integral(CommandFieldsAsk::c_OTHER_IP)];
        int port = std::stoi(received_cmd[to_integral(CommandFieldsAsk::c_OTHER_CLIENT_PORT)]);
        std::string ip_port = get_ip_port(ip, port);

        if (!nodes_connections_.contains(ip_port)) {
            if (!connect_to_node(ip, port)) {
                return false;
            }
        }
        return true;
    }

    bool Client::handle_no_ask_error(Command& received_cmd) {
        std::string ip = received_cmd[to_integral(CommandFieldsNoAskingError::c_OTHER_IP)];
        int port = std::stoi(received_cmd[to_integral(CommandFieldsNoAskingError::c_OTHER_CLIENT_PORT)]);
        std::string ip_port = get_ip_port(ip, port);

        if (!nodes_connections_.contains(ip_port)) {
            if (!connect_to_node(ip, port)) {
                return false;
            }
        }
        return true;
    }

    //TODO: Handle case when server shuts down, connection still remains then receive_meta_data blocks
    Status Client::get_value(observer_ptr<net::Connection> link, const std::string& key, ByteArray& value, int offset, int size, bool asking) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;

        //No node available
        if (link == nullptr) {
            return Status::new_error("Not connected to any node");
        }

        std::string asking_string = asking ? "true" : "false";
        Command cmd{ key, std::to_string(size), std::to_string(offset), asking_string }; //Size 0 means get all
        send_instruction(*link, cmd, Instruction::c_GET);

        //handle response
        MetaData received_meta_data = get_metadata(*link);
        Command received_cmd = get_command(*link, received_meta_data.argc, received_meta_data.command_size);

        switch (received_meta_data.instruction) {
        case Instruction::c_GET_RESPONSE:
        {
            uint64_t total_payload_size = received_meta_data.payload_size;
            uint64_t current_payload_size = std::stoull(received_cmd[to_integral(CommandFieldsGetResponse::c_SIZE)]);
            uint64_t current_offset = std::stoull(received_cmd[to_integral(CommandFieldsGetResponse::c_OFFSET)]);

            value.resize(total_payload_size);
            get_payload(*link, value.data() + current_offset, current_payload_size);
            return Status::new_ok();
        }

        case Instruction::c_ERROR_RESPONSE:
        {
            ByteArray received_payload = get_payload(*link, received_meta_data.payload_size);
            return Status::new_error(received_payload.to_string());
        }

        case Instruction::c_MOVE:
        {
            if (!handle_move(received_cmd, slot_number)) {
                return Status::new_error("Could not connect to new node");
            }
            return get_value(key, value, offset, size);
        }

        case Instruction::c_ASK:
        {
            if (!handle_ask(received_cmd)) {
                return Status::new_error("Could not connect to new node");
            }
            std::string other_ip = received_cmd[to_integral(CommandFieldsAsk::c_OTHER_IP)];
            std::string other_port = received_cmd[to_integral(CommandFieldsAsk::c_OTHER_CLIENT_PORT)];
            std::string ip_port = get_ip_port(other_ip, std::stoi(other_port));
            observer_ptr<net::Connection> new_link = &nodes_connections_[ip_port];
            return get_value(new_link, key, value, offset, size, true);
        }


        case Instruction::c_NO_ASKING_ERROR:
        {
            if (!handle_no_ask_error(received_cmd)) {
                return Status::new_error("Could not connect to new node");
            }
            std::string other_ip = received_cmd[to_integral(CommandFieldsNoAskingError::c_OTHER_IP)];
            std::string other_port = received_cmd[to_integral(CommandFieldsNoAskingError::c_OTHER_CLIENT_PORT)];
            std::string ip_port = get_ip_port(other_ip, std::stoi(other_port));
            observer_ptr<net::Connection> new_link = &nodes_connections_[ip_port];
            return get_value(new_link, key, value, offset, size, false);
        }

        default:
        {
            return Status::new_unknown_response("Unknown response");
        }
        }
    }

    Status Client::get_value(const std::string& key, ByteArray& value, int offset, int size) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;
        observer_ptr<net::Connection> link = get_node_connection_by_slot(slot_number);

        return get_value(link, key, value, offset, size, false);
    }

    Status Client::erase_value(observer_ptr<net::Connection> link, const std::string& key) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;

        //No node available
        if (link == nullptr) {
            return Status::new_error("Not connected to any node");
        }

        Command cmd{ key };
        send_instruction(*link, cmd, Instruction::c_ERASE);

        //handle response
        ResponseData response = get_response(*link);
        MetaData& received_meta_data = std::get<to_integral(ResponseDataFields::c_METADATA)>(response);
        Command& received_cmd = std::get<to_integral(ResponseDataFields::c_COMMAND)>(response);
        ByteArray& received_payload = std::get<to_integral(ResponseDataFields::c_PAYLOAD)>(response);

        switch (received_meta_data.instruction) {
        case Instruction::c_OK_RESPONSE:
        {
            return Status::new_ok();
        }

        case Instruction::c_ERROR_RESPONSE:
        {
            return Status::new_error(received_payload.to_string());
        }

        case Instruction::c_MOVE:
        {
            if (!handle_move(received_cmd, slot_number)) {
                return Status::new_error("Could not connect to new node");
            }
            return erase_value(key);
        }

        case Instruction::c_ASK:
        {
            if (!handle_ask(received_cmd)) {
                return Status::new_error("Could not connect to new node");
            }
            std::string other_ip = received_cmd[to_integral(CommandFieldsAsk::c_OTHER_IP)];
            std::string other_port = received_cmd[to_integral(CommandFieldsAsk::c_OTHER_CLIENT_PORT)];
            std::string ip_port = get_ip_port(other_ip, std::stoi(other_port));
            observer_ptr<net::Connection> new_link = &nodes_connections_[ip_port];
            return erase_value(new_link, key);
        }

        default: {
            return Status::new_unknown_response("Unknown response");
        }
        }
    }

    Status Client::erase_value(const std::string& key) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;
        observer_ptr<net::Connection> link = get_node_connection_by_slot(slot_number);

        return erase_value(link, key);
    }

}  // namespace client