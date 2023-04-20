#include <iostream>
#include <sstream>
#include <variant>

#include "Client.hpp"
#include "../node/Cluster.hpp"
#include "../utils/ByteArray.hpp"
#include "../utils/Status.hpp"

using Client = client::Client;

struct Command {};
struct DisconnectCommand: public Command {};
struct UpdateSlotInfoCommand: public Command {};
struct UnknownCommand: public Command {};
struct InvalidArgsCommand: public Command {};
struct ConnectCommand: public Command {
    std::string ip;
    uint16_t client_port;
};
struct PutCommand: public Command {
    std::string key;
    std::string value;
};
struct GetCommand: public Command {
    std::string key;
    uint64_t size;
    uint64_t offset;
};
struct EraseCommand: public Command {
    std::string key;
};
struct MigrateSlotCommand: public Command {
    uint16_t slot;
    std::string ip;
    uint16_t client_port;
};
struct ImportSlotCommand: public Command {
    uint16_t slot;
    std::string ip;
    uint16_t client_port;
};
struct AddNodeToClusterCommand: public Command {
    std::string name;
    std::string ip;
    uint16_t client_port;
    uint16_t cluster_port;
};

using CommandVariant = std::variant<DisconnectCommand,
    UpdateSlotInfoCommand,
    UnknownCommand,
    InvalidArgsCommand,
    ConnectCommand,
    PutCommand,
    GetCommand,
    EraseCommand,
    MigrateSlotCommand,
    ImportSlotCommand,
    AddNodeToClusterCommand>;

std::string prompt(const std::string& message) {
    std::cout << message << std::fflush;
    std::string input;
    getline(std::cin, input);
    return input;
}

template<typename... Types>
std::tuple<Types...> parse_input(std::istringstream& stream) {
    return std::tuple<Types...>{parse_next<Types>(stream)...};
}

template<typename T>
T parse_next(std::istringstream& stream) {
    T value;
    stream >> value;
    if (stream.fail()) {
        throw std::runtime_error("Invalid input");
    }
    return value;
}

class CommandVisitor {
public:

    CommandVisitor(observer_ptr<Client> client, observer_ptr<ByteArray> payload): client_{ client }, payload_{ payload } {}

    Status operator() (const DisconnectCommand& command) {
        client_->disconnect_all();
        return Status::new_ok();
    }

    Status operator() (const UpdateSlotInfoCommand& command) {
        return client_->get_update_slot_info();
    }

    Status operator() (const ConnectCommand& command) {
        return client_->connect_to_node(command.ip, command.client_port);
    }

    Status operator() (const PutCommand& command) {
        return client_->put_value(command.key, command.value);
    }

    Status operator() (const GetCommand& command) {
        return client_->get_value(command.key, *payload_, command.size, command.offset);
    }

    Status operator() (const EraseCommand& command) {
        return client_->erase_value(command.key);
    }

    Status operator() (const MigrateSlotCommand& command) {
        return client_->migrate_slot(command.slot, command.ip, command.client_port);
    }

    Status operator() (const ImportSlotCommand& command) {
        return client_->import_slot(command.slot, command.ip, command.client_port);
    }

    Status operator() (const AddNodeToClusterCommand& command) {
        return client_->add_node_to_cluster(command.name, command.ip, command.client_port, command.cluster_port);
    }

    Status operator() (const InvalidArgsCommand& command) {
        return Status::new_error("Invalid arguments");
    }

    Status operator() (const UnknownCommand& command) {
        return Status::new_error("Unknown command. Type 'help' for a list of available commands.");
    }

    observer_ptr<Client> client_;
    observer_ptr<ByteArray> payload_;
};

CommandVariant extract_arguments(const std::string& command, std::istringstream& stream) {
    try {
        if (command == "connect")
        {
            auto args = parse_input<std::string, uint16_t>(stream);
            return ConnectCommand{ {}, std::get<0>(args), std::get<1>(args) };
        }

        else if (command == "put")
        {
            auto args = parse_input<std::string, std::string>(stream);
            return PutCommand{ {}, std::get<0>(args), std::get<1>(args) };
        }

        else if (command == "get")
        {
            auto args = parse_input<std::string, uint64_t, uint64_t>(stream);
            return GetCommand{ {}, std::get<0>(args), std::get<1>(args), std::get<2>(args) };
        }

        else if (command == "erase")
        {
            auto args = parse_input<std::string>(stream);
            return EraseCommand{ {}, std::get<0>(args) };
        }

        else if (command == "migrate_slot")
        {
            auto args = parse_input<uint16_t, std::string, uint16_t>(stream);
            return MigrateSlotCommand{ {}, std::get<0>(args), std::get<1>(args), std::get<2>(args) };
        }

        else if (command == "import_slot")
        {
            auto args = parse_input<uint16_t, std::string, uint16_t>(stream);
            return ImportSlotCommand{ {}, std::get<0>(args), std::get<1>(args), std::get<2>(args) };
        }

        else if (command == "add_node_to_cluster")
        {
            auto args = parse_input<std::string, std::string, uint16_t, uint16_t>(stream);
            return AddNodeToClusterCommand{ {}, std::get<0>(args), std::get<1>(args), std::get<2>(args), std::get<3>(args) };
        }

        else if (command == "disconnect")
        {
            return DisconnectCommand{};
        }

        else if (command == "update_slot_info")
        {
            return UpdateSlotInfoCommand{};
        }

        else {
            return UnknownCommand{};
        }
    }
    catch (std::runtime_error& e) {
        return InvalidArgsCommand{};
    }
}

Status execute_command(observer_ptr<Client> client, observer_ptr<ByteArray> payload, const std::string& command, std::istringstream& stream) {
    CommandVariant command_variant = extract_arguments(command, stream);
    CommandVisitor visitor{ client, payload };
    return std::visit(visitor, command_variant);
}

int main(int argc, char** argv) {
    client::Client client{};

    while (true) {
        std::string read_line = prompt("> ");
        std::istringstream stream{ read_line };
        std::string command;
        stream >> command;

        Status status = Status::new_ok();
        ByteArray payload{};

        if (command == "exit") {
            break;
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
            status = execute_command(&client, &payload, command, stream);
        }

        if (!status.is_ok()) {
            std::cout << "Error: " << status.get_msg() << std::endl;
        }
        else if (payload.size() != 0) {
            std::cout << "Payload: " << payload.to_string() << std::endl;
        }
    }
}