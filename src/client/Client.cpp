#include "Client.hpp"

#include <random>
#include <stdexcept>
#include <sstream>

using namespace node::protocol;

namespace client {

    std::string get_ip_port(const std::string& ip, uint16_t port) {
        return ip + ":" + std::to_string(port);
    }
    std::string get_ip_port(const std::string& ip, const std::string& port) {
        return ip + ":" + port;
    }

    Status Client::connect_to_node(const std::string& ip, uint16_t port) {
        net::Socket socket{};
        std::string ip_port = get_ip_port(ip, port);
        try {
            nodes_connections_.emplace(ip_port, socket.connect(ip, port));
        }
        catch (std::runtime_error& e) {
            return Status::new_error(e.what());
        }
        return Status::new_ok();
    }

    void Client::disconnect_all() {
        nodes_connections_.clear();
    }

    observer_ptr<net::Connection> Client::get_random_connection() {
        if (nodes_connections_.size() == 0) {
            return nullptr;
        }
        std::mt19937 random_engine(std::random_device{}());
        std::uniform_int_distribution<uint16_t> dist(0, nodes_connections_.size() - 1); //Important: [a, b] both borders inclusive
        uint16_t random_index = dist(random_engine);
        auto ret = &std::next(nodes_connections_.begin(), random_index)->second;
        return ret;
    }

    ResponseData get_response(net::Connection& connection) {
        MetaData received_meta_data = get_metadata(connection, "Client");
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

    //This function is called if a node sends a MOVE instruction
    //That happends when a slot has been moved from a node to another node
    //The response of the MOVE instruction has the information of the new node
    //This function connects to the new node and updates the slot info
    bool Client::handle_move(Command& received_cmd, uint16_t slot) {
        std::string ip = received_cmd[to_integral(CommandFieldsMove::c_OTHER_IP)];
        uint16_t port = std::stoi(received_cmd[to_integral(CommandFieldsMove::c_OTHER_CLIENT_PORT)]);
        std::string ip_port = get_ip_port(ip, port);

        if (!nodes_connections_.contains(ip_port)) {
            if (!connect_to_node(ip, port).is_ok()) {
                return false;
            }
        }
        slots_nodes_[slot] = ip_port;
        return true;
    }

    //This function retrieves the connection of the node that handles the given slot
    observer_ptr<net::Connection> Client::get_node_connection_by_slot(uint16_t slot_number) {
        observer_ptr<net::Connection> link = nullptr;
        //Slot handled by unknown node, but another random node available
        if (!nodes_connections_.empty() && (slots_nodes_[slot_number].empty() || !nodes_connections_.contains(slots_nodes_[slot_number]))) {
            link = get_random_connection();
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
        ResponseData response;
        try {
            response = get_response(*link);
        }
        catch (std::exception& e) {
            return Status::new_error(e.what());
        }

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

        //In this case, the node that received the PUT instruction is not the node that handles the slot yet,
        //but the value should be set in the new node
        //Therefore we cannot update the slot info yet and therefore have to send the instruction to the other node manually
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

    //This function is called if a node sends an ASK instruction
    //That happens when a slot is in the process of being moved to another node
    //This function connects to the new node
    bool Client::handle_ask(Command& received_cmd) {
        std::string ip = received_cmd[to_integral(CommandFieldsAsk::c_OTHER_IP)];
        uint16_t port = std::stoi(received_cmd[to_integral(CommandFieldsAsk::c_OTHER_CLIENT_PORT)]);
        std::string ip_port = get_ip_port(ip, port);

        if (!nodes_connections_.contains(ip_port)) {
            if (!connect_to_node(ip, port).is_ok()) {
                return false;
            }
        }
        return true;
    }

    //This function is called if a node sends a NO_ASKING_ERROR instruction
    //That happens when a slot is in the process of being moved to another node and a GET instruction is sent to the old node
    //or to the new node without the ask flag
    //This function connects to the new node
    bool Client::handle_no_ask_error(Command& received_cmd) {
        std::string ip = received_cmd[to_integral(CommandFieldsNoAskingError::c_OTHER_IP)];
        uint16_t port = std::stoi(received_cmd[to_integral(CommandFieldsNoAskingError::c_OTHER_CLIENT_PORT)]);
        std::string ip_port = get_ip_port(ip, port);

        if (!nodes_connections_.contains(ip_port)) {
            if (!connect_to_node(ip, port).is_ok()) {
                return false;
            }
        }
        return true;
    }

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
        MetaData received_meta_data;
        Command received_cmd;
        try {
            received_meta_data = get_metadata(*link, "Get failed");
            received_cmd = get_command(*link, received_meta_data.argc, received_meta_data.command_size);
        }
        catch (std::exception& e) {
            return Status::new_error(e.what());
        }

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

    Status Client::erase_value(observer_ptr<net::Connection> link, const std::string& key, bool asking) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;
        //No node available
        if (link == nullptr) {
            return Status::new_error("Not connected to any node");
        }

        std::string asking_string = asking ? "true" : "false";
        Command cmd{ key, asking_string };
        send_instruction(*link, cmd, Instruction::c_ERASE);

        //handle response
        ResponseData response;
        try {
            response = get_response(*link);
        }
        catch (std::exception& e) {
            return Status::new_error(e.what());
        }

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
            return erase_value(new_link, key, true);
        }

        default: {
            return Status::new_unknown_response("Unknown response");
        }
        }
    }

    Status Client::erase_value(const std::string& key) {
        uint16_t slot_number = node::cluster::get_key_hash(key) % node::cluster::CLUSTER_AMOUNT_OF_SLOTS;
        observer_ptr<net::Connection> link = get_node_connection_by_slot(slot_number);

        return erase_value(link, key, false);
    }

    //This function takes in a string of the form
    //slot_number_begin slot_number_end ip:port
    //and updates the slot info
    void Client::update_slot_info(ByteArray& data) {
        std::stringstream stream(data.to_string());
        std::string current_line;

        //Split the string by new line
        while (std::getline(stream, current_line, '\n')) {
            //Split the string by space
            std::stringstream current_line_ss(current_line);

            //Extract the slot numbers and the ip:port
            uint16_t slot_number_begin, slot_number_end;
            std::string ip_port;
            current_line_ss >> slot_number_begin;
            current_line_ss >> slot_number_end;
            current_line_ss >> ip_port;
            
            //Update the slot info
            for (uint16_t slot_number = slot_number_begin; slot_number <= slot_number_end; ++slot_number) {
                if (ip_port != "NULL") {
                    slots_nodes_[slot_number] = ip_port;
                }
                else {
                    slots_nodes_[slot_number] = "";
                }
            }
        }
    }

    //This function makes the request to a random node to get the slot info of the cluster
    Status Client::get_update_slot_info() {
        observer_ptr<net::Connection> link = get_random_connection();
        if (link == nullptr) {
            return Status::new_error("Not connected to any node");
        }

        Command cmd{};
        send_instruction(*link, cmd, Instruction::c_GET_SLOTS);

        //handle response
        ResponseData response;
        try {
            response = get_response(*link);
        }
        catch (std::exception& e) {
            return Status::new_error(e.what());
        }

        MetaData& received_meta_data = std::get<to_integral(ResponseDataFields::c_METADATA)>(response);
        Command& received_cmd = std::get<to_integral(ResponseDataFields::c_COMMAND)>(response);
        ByteArray& received_payload = std::get<to_integral(ResponseDataFields::c_PAYLOAD)>(response);

        switch (received_meta_data.instruction) {
        case Instruction::c_ERROR_RESPONSE:
        {
            return Status::new_error(received_payload.to_string());
        }

        case Instruction::c_OK_RESPONSE:
        {
            update_slot_info(received_payload);
            return Status::new_ok();
        }

        default:
        {
            return Status::new_unknown_response("Unknown response");
        }
        }
    }

    //This function is called to connect to a partner node and send it the instruction to migrate or import a slot
    Status Client::handle_slot_migration(uint16_t slot, const std::string& partner_ip, int partner_port, Instruction instruction) {
        std::string& partner_ip_port = slots_nodes_[slot];
        if (partner_ip_port == "") {
            return Status::new_error("Slot is not handled by a known node");
        }

        observer_ptr<net::Connection> partner_link = nullptr;
        if (nodes_connections_.contains(partner_ip_port)) {
            partner_link = &nodes_connections_[partner_ip_port];
        }

        //Connect to partner node if not already connected
        if (partner_link == nullptr) {
            std::string ip = partner_ip_port.substr(0, partner_ip_port.find(":"));
            uint16_t port = std::stoi(partner_ip_port.substr(partner_ip_port.find(":") + 1));
            if (!connect_to_node(ip, port).is_ok()) {
                return Status::new_error("Could not connect to node");
            }
            partner_link = &nodes_connections_[partner_ip_port];
        }

        Command cmd{ std::to_string(slot), partner_ip, std::to_string(partner_port) };
        send_instruction(*partner_link, cmd, instruction);

        //handle response
        ResponseData response;
        try {
            response = get_response(*partner_link);
        }
        catch (std::exception& e) {
            return Status::new_error(e.what());
        }

        MetaData& received_meta_data = std::get<to_integral(ResponseDataFields::c_METADATA)>(response);
        Command& received_cmd = std::get<to_integral(ResponseDataFields::c_COMMAND)>(response);
        ByteArray& received_payload = std::get<to_integral(ResponseDataFields::c_PAYLOAD)>(response);

        switch (received_meta_data.instruction) {
        case Instruction::c_ERROR_RESPONSE:
        {
            return Status::new_error(received_payload.to_string());
        }

        case Instruction::c_OK_RESPONSE:
        {
            return Status::new_ok();
        }

        case Instruction::c_MOVE:
        {
            if (!handle_move(received_cmd, slot)) {
                return Status::new_error("Could not connect to new node");
            }
            return handle_slot_migration(slot, partner_ip, partner_port, instruction);
        }

        default:
        {
            return Status::new_unknown_response("Unknown response");
        }
        }
    }

    //This function is called when a slot is migrated from one node to another
    Status Client::migrate_slot(uint16_t slot, const std::string& importing_ip, int importing_port) {
        std::string& serving_node_ip_port = slots_nodes_[slot];
        if (serving_node_ip_port == "") {
            return Status::new_error("Slot is not handled by a known node");
        }

        observer_ptr<net::Connection> serving_node_link = nullptr;
        if (nodes_connections_.contains(serving_node_ip_port)) {
            serving_node_link = &nodes_connections_[serving_node_ip_port];
        }

        if (serving_node_link == nullptr) {
            std::string ip = serving_node_ip_port.substr(0, serving_node_ip_port.find(":"));
            uint16_t port = std::stoi(serving_node_ip_port.substr(serving_node_ip_port.find(":") + 1));
            if (!connect_to_node(ip, port).is_ok()) {
                return Status::new_error("Could not connect to node");
            }
            serving_node_link = &nodes_connections_[serving_node_ip_port];
        }

        Command cmd{ std::to_string(slot), importing_ip, std::to_string(importing_port) };
        send_instruction(*serving_node_link, cmd, Instruction::c_MIGRATE_SLOT);

        //handle response
        ResponseData response;
        try {
            response = get_response(*serving_node_link);
        }
        catch (std::exception& e) {
            return Status::new_error(e.what());
        }

        MetaData& received_meta_data = std::get<to_integral(ResponseDataFields::c_METADATA)>(response);
        Command& received_cmd = std::get<to_integral(ResponseDataFields::c_COMMAND)>(response);
        ByteArray& received_payload = std::get<to_integral(ResponseDataFields::c_PAYLOAD)>(response);

        switch (received_meta_data.instruction) {
        case Instruction::c_ERROR_RESPONSE:
        {
            return Status::new_error(received_payload.to_string());
        }

        case Instruction::c_OK_RESPONSE:
        {
            return Status::new_ok();
        }

        case Instruction::c_MOVE:
        {
            if (!handle_move(received_cmd, slot)) {
                return Status::new_error("Could not connect to new node");
            }
            return handle_slot_migration(slot, importing_ip, importing_port, Instruction::c_IMPORT_SLOT);
        }

        default:
        {
            return Status::new_unknown_response("Unknown response");
        }
        }
    }

    //This function is called if a slot is imported from another node
    Status Client::import_slot(uint16_t slot, const std::string& importing_ip, int importing_port) {
        std::string& serving_node_ip_port = slots_nodes_[slot];
        if (serving_node_ip_port == "") {
            return Status::new_error("Slot is not handled by a known node");
        }

        std::string importing_ip_port = get_ip_port(importing_ip, importing_port);
        observer_ptr<net::Connection> importing_link = nullptr;
        if (nodes_connections_.contains(importing_ip_port)) {
            importing_link = &nodes_connections_[importing_ip_port];
        }

        if (importing_link == nullptr) {
            if (!connect_to_node(importing_ip, importing_port).is_ok()) {
                return Status::new_error("Could not connect to node");
            }
            importing_link = &nodes_connections_[importing_ip_port];
        }


        std::string serving_node_ip = serving_node_ip_port.substr(0, serving_node_ip_port.find(":"));
        uint16_t serving_node_port = std::stoi(serving_node_ip_port.substr(serving_node_ip_port.find(":") + 1));

        Command cmd{ std::to_string(slot), serving_node_ip, std::to_string(serving_node_port) };
        send_instruction(*importing_link, cmd, Instruction::c_IMPORT_SLOT);

        //handle response
        ResponseData response;
        try {
            response = get_response(*importing_link);
        }
        catch (std::exception& e) {
            return Status::new_error(e.what());
        }

        MetaData& received_meta_data = std::get<to_integral(ResponseDataFields::c_METADATA)>(response);
        Command& received_cmd = std::get<to_integral(ResponseDataFields::c_COMMAND)>(response);
        ByteArray& received_payload = std::get<to_integral(ResponseDataFields::c_PAYLOAD)>(response);

        switch (received_meta_data.instruction) {
        case Instruction::c_ERROR_RESPONSE:
        {
            return Status::new_error(received_payload.to_string());
        }

        case Instruction::c_OK_RESPONSE:
        {
            return Status::new_ok();
        }

        case Instruction::c_MOVE:
        {
            if (!handle_move(received_cmd, slot)) {
                return Status::new_error("Could not connect to new node");
            }
            return handle_slot_migration(slot, serving_node_ip, serving_node_port, Instruction::c_IMPORT_SLOT);
        }

        default:
        {
            return Status::new_unknown_response("Unknown response");
        }
        }
        return handle_slot_migration(slot, importing_ip, importing_port, Instruction::c_IMPORT_SLOT);
    }

    //This function adds a node to the cluster by sending a MEET instruction to a random node
    Status Client::add_node_to_cluster(const std::string& name, const std::string& ip, uint16_t client_port, uint16_t cluster_port) {
        observer_ptr<net::Connection> link = get_random_connection();
        if (link == nullptr) {
            return Status::new_error("Not connected to any node");
        }

        Command cmd{ ip, std::to_string(client_port), std::to_string(cluster_port), name };
        send_instruction(*link, cmd, Instruction::c_MEET);

        //handle response
        ResponseData response;
        try {
            response = get_response(*link);
        }
        catch (std::exception& e) {
            return Status::new_error(e.what());
        }

        MetaData& received_meta_data = std::get<to_integral(ResponseDataFields::c_METADATA)>(response);
        Command& received_cmd = std::get<to_integral(ResponseDataFields::c_COMMAND)>(response);
        ByteArray& received_payload = std::get<to_integral(ResponseDataFields::c_PAYLOAD)>(response);

        switch (received_meta_data.instruction) {
        case Instruction::c_ERROR_RESPONSE:
        {
            return Status::new_error(received_payload.to_string());
        }
        case Instruction::c_OK_RESPONSE:
        {
            //Connect to new node
            if (!connect_to_node(ip, client_port).is_ok()) {
                return Status::new_error("Could not connect to node");
            }
            return Status::new_ok();
        }
        default:
        {
            return Status::new_unknown_response("Unknown response");
        }
        }
    }
}  // namespace client