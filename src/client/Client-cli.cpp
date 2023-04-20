#include <iostream>

#include "Client.hpp"
#include "../utils/ByteArray.hpp"
#include "../utils/Status.hpp"

template<typename T = std::string>
T prompt(const std::string& message) {
    std::cout << message << std::flush;
    T input;
    std::cin >> input;
    return input;
}

int main(int argc, char** argv) {
    client::Client client{};

    while (true) {
        std::string command = prompt("> ");
        Status status = Status::new_ok();
        ByteArray payload{};

        if (command == "exit") {
            break;
        }
        else if (command == "connect") {
            std::string ip = prompt("Enter ip: ");
            uint16_t client_port = prompt<uint16_t>("Enter client port: ");
            status = client.connect_to_node(ip, client_port);
        }
        else if (command == "disconnect") {
            client.disconnect_all();
        }
        else if (command == "put") {
            std::string key = prompt("Enter key: ");
            std::string value = prompt("Enter value: ");
            status = client.put_value(key, value);
        }
        else if (command == "get") {
            std::string key = prompt("Enter key: ");
            uint64_t size = prompt<uint64_t>("Enter size: ");
            uint64_t offset = prompt<uint64_t>("Enter offset: ");
            status = client.get_value(key, payload, offset, size);
        }
        else if (command == "erase") {
            std::string key = prompt("Enter key: ");
            status = client.erase_value(key);
        }
        else if (command == "update_slot_info") {
            status = client.get_update_slot_info();
        }
        else if (command == "migrate_slot") {
            uint16_t slot = prompt<uint16_t>("Enter slot: ");
            std::string ip = prompt("Enter ip: ");
            uint16_t client_port = prompt<uint16_t>("Enter client port: ");
            status = client.migrate_slot(slot, ip, client_port);
        }
        else if (command == "import slot") {
            uint16_t slot = prompt<uint16_t>("Enter slot: ");
            std::string ip = prompt("Enter ip: ");
            uint16_t client_port = prompt<uint16_t>("Enter client port: ");
            status = client.import_slot(slot, ip, client_port);
        }
        else if (command == "add_node_to_cluster") {
            std::string name = prompt("Enter name: ");
            std::string ip = prompt("Enter ip: ");
            uint16_t client_port = prompt<uint16_t>("Enter client port: ");
            uint16_t cluster_port = prompt<uint16_t>("Enter cluster port: ");
            status = client.add_node_to_cluster(name, ip, client_port, cluster_port);
        }
        else if (command == "help") {
            std::cout << "Available commands:" << std::endl;
            std::cout << "connect <ip> - connect to a server" << std::endl;
            std::cout << "disconnect - disconnect from the server" << std::endl;
            std::cout << "put <key> <value> - put a key-value pair" << std::endl;
            std::cout << "get <key> <size> <offset> - get the value of a key" << std::endl;
            std::cout << "erase <key> - delete a key-value pair" << std::endl;
            std::cout << "update_slot_info - update the slot info from the cluster for faster requests" << std::endl;
            std::cout << "migrate_slot <slot> <other_ip> <other_client_port> - migrate a slot to another node" << std::endl;
            std::cout << "import_slot <slot> <other_ip> <other_client_port> - import a slot from another node" << std::endl;
            std::cout << "add_node_to_cluster <name> <ip> <client_port> <cluster_port> - add a node to the cluster" << std::endl;
            std::cout << "help - show this help" << std::endl;
            std::cout << "exit - exit the program" << std::endl;
        }
        else {
            std::cout << "Unknown command. Type 'help' for a list of available commands." << std::endl;
        }

        if (!status.is_ok()) {
            std::cout << "Error: " << status.get_msg() << std::endl;
        }
        else if (payload.size() != 0) {
            std::cout << "Payload: " << payload.to_string() << std::endl;
        }
    }
}