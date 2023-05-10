#include <cassert>

#include "node.hpp"
#include "InstructionHandler.hpp"
#include "Cluster.hpp"
#include "../KVS/IKeyValueStore.hpp"
#include "../KVS/InMemoryKVS.hpp"
#include "../net/Connection.hpp"
#include "../net/Socket.hpp"

using MetaData = node::protocol::MetaData;
using command = node::protocol::Command;
using Instruction = node::protocol::Instruction;

namespace node {

    Node::~Node() {
        stop();
    }

    Node::Node(
        std::unique_ptr<key_value_store::IKeyValueStore> kvs,
        uint16_t client_port,
        uint16_t cluster_port,
        std::array<char, cluster::CLUSTER_NAME_LEN> name,
        std::array<char, cluster::CLUSTER_IP_LEN> ip,
        bool serve_all_slots
    ) {
        kvs_ = std::move(kvs);
        client_port_ = client_port;
        cluster_port_ = cluster_port;
        name_ = name;

        //Initialize cluster state
        cluster_state_.slots.resize(cluster::CLUSTER_AMOUNT_OF_SLOTS);
        cluster_state_.myself.client_port = client_port;
        cluster_state_.myself.cluster_port = cluster_port;
        cluster_state_.myself.name = name;
        cluster_state_.myself.ip = ip;
        cluster_state_.size = 0;

        if (!serve_all_slots) {
            return;
        }

        //Only start serving slots if specified, used for the first node of the cluster
        for (int slot = 0; slot < cluster::CLUSTER_AMOUNT_OF_SLOTS; slot++) {
            cluster_state_.slots[slot].amount_of_keys = 0;
            cluster_state_.slots[slot].migration_partner = nullptr;
            cluster_state_.slots[slot].state = cluster::SlotState::c_NORMAL;
            cluster_state_.slots[slot].served_by = &cluster_state_.myself;
            cluster_state_.myself.served_slots[slot] = true;
        }
        cluster_state_.myself.num_slots_served = cluster::CLUSTER_AMOUNT_OF_SLOTS;
        cluster_state_.part_of_cluster = true;
    }

    Node Node::new_in_memory_node(std::string name, uint16_t client_port, uint16_t cluster_port, std::string ip, bool serve_all_slots) {
        assert(name.size() <= cluster::CLUSTER_NAME_LEN); //TODO: Check nullterminal
        assert(ip.size() <= cluster::CLUSTER_IP_LEN); //TODO: Check nullterminal

        std::array<char, cluster::CLUSTER_NAME_LEN> name_arr{};
        std::copy(name.begin(), name.end(), name_arr.begin());

        std::array<char, cluster::CLUSTER_IP_LEN> ip_arr{};
        std::copy(ip.begin(), ip.end(), ip_arr.begin());

        return Node{ std::make_unique<key_value_store::InMemoryKVS>(), client_port, cluster_port, name_arr, ip_arr, serve_all_slots };
    }

    void Node::main_loop() {
        net::Socket client_socket{}, cluster_socket{};

        client_socket.set_non_blocking();
        cluster_socket.set_non_blocking();

        client_socket.set_keep_alive();
        cluster_socket.set_keep_alive();

        client_socket.listen(client_port_);
        cluster_socket.listen(cluster_port_);

        connections_epoll_.add_event(client_socket.fd(), EPOLLIN | EPOLLET);
        connections_epoll_.add_event(cluster_socket.fd(), EPOLLIN | EPOLLET);

        while (running_) {
            int num_ready = connections_epoll_.wait(NODE_WAIT_TIMEOUT);
            if (!running_) {
                break;
            }

            for (int i = 0; i < num_ready; i++) {
                int fd = connections_epoll_.get_event_fd(i);

                //New connection
                if (fd == client_socket.fd() || fd == cluster_socket.fd() && !fd_to_connection_.contains(fd)) {
                    net::Socket& receiving_socket = fd == client_socket.fd() ? client_socket : cluster_socket;
                    try {
                        net::Connection connection = receiving_socket.accept();
                        connections_epoll_.add_event(connection.fd(), EPOLLIN | EPOLLET);
                        fd_to_connection_[connection.fd()] = connection;
                    }
                    catch (std::exception& e) {
                        continue;
                    }
                }

                //Existing connection
                else if (connections_epoll_.get_events()[i].events & EPOLLIN) {
                    net::Connection& connection = fd_to_connection_[fd];
                    //std::unique_lock lock{cluster_state_mutex_};
                    handle_connection(connection);
                }
            }
        }
    }

    void Node::gossip() {
        while (gossiping_) {
            if (cluster_state_.part_of_cluster) {
                cluster::send_ping(cluster_state_);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(NODE_PING_PAUSE));
        }
    }

    void Node::execute_instruction(net::Connection& connection, const MetaData& meta_data, const command& command) {
        switch (meta_data.instruction) {
        case Instruction::c_PUT:
            instruction_handler::handle_put(connection, meta_data, command, get_kvs(), cluster_state_);
            break;
        case Instruction::c_GET:
            instruction_handler::handle_get(connection, command, get_kvs(), cluster_state_);
            break;
        case Instruction::c_ERASE:
            instruction_handler::handle_erase(connection, command, get_kvs(), cluster_state_);
            break;
        case Instruction::c_MEET:
            instruction_handler::handle_meet(connection, command, cluster_state_);
            break;
        case Instruction::c_MIGRATE_SLOT:
            instruction_handler::handle_migrate_slot(connection, command, cluster_state_);
            break;
        case Instruction::c_IMPORT_SLOT:
            instruction_handler::handle_import_slot(connection, command, cluster_state_);
            break;
        case Instruction::c_CLUSTER_MIGRATION_FINISHED:
            instruction_handler::handle_migration_finished(command, cluster_state_);
            break;
        case Instruction::c_GET_SLOTS:
            instruction_handler::handle_get_slots(connection, command, cluster_state_);
            break;
        case Instruction::c_CLUSTER_PING:
            cluster::handle_ping(connection, cluster_state_, command);
            break;
        default:
            protocol::send_instruction(connection, Status::new_not_supported("Unknown instruction"));
            break;
        }
    }

    void Node::disconnect(net::Connection& connection) {
        if (connection.fd() == -1 || !fd_to_connection_.contains(connection.fd())) {
            return;
        }
        connections_epoll_.remove_event(connection.fd());
        fd_to_connection_.erase(connection.fd());
    }

    void Node::handle_connection(net::Connection& connection) {
        try {
            MetaData meta_data = node::protocol::get_metadata(connection, std::string(cluster_state_.myself.name.data()));
            command command = node::protocol::get_command(connection, meta_data.argc, meta_data.command_size);
            execute_instruction(connection, meta_data, command);
        }
        catch (const std::exception& e) {
            disconnect(connection);
            return;
        }
    }
}
