#include "node.hpp"
#include "InstructionHandler.hpp"
#include "../KVS/IKeyValueStore.hpp"
#include "../KVS/InMemoryKVS.hpp"
#include "../net/Connection.hpp"
#include "../net/Socket.hpp"

using MetaData = node::protocol::MetaData;
using command = node::protocol::Command;
using Instruction = node::protocol::Instruction;

namespace node {

    Node::Node(std::unique_ptr<key_value_store::IKeyValueStore> kvs, cluster::ClusterState state, int client_port, int cluster_port) {
        kvs_ = std::move(kvs);
        cluster_state_ = state;
        client_port_ = client_port;
        cluster_port_ = cluster_port;
        cluster_state_.slots.resize(cluster::CLUSTER_AMOUNT_OF_SLOTS);
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
                connections_epoll_.reset_occurred_events();

                //New connection
                if (fd == client_socket.fd() || fd == cluster_socket.fd()) {
                    net::Connection connection = client_socket.accept();
                    connections_epoll_.add_event(connection.fd(), EPOLLIN | EPOLLET);
                    fd_to_connection_[connection.fd()] = connection;
                }

                //Existing connection
                else if (connections_epoll_.get_events()[i].events & EPOLLIN) {
                    net::Connection& connection = fd_to_connection_[fd];
                    handle_connection(connection);
                }
            }
        }
    }

    void Node::execute_instruction(net::Connection& connection, const MetaData& meta_data, const command& command) {
        switch (meta_data.instruction) {
        case Instruction::c_PUT:
            instruction_handler::handle_put(connection, meta_data, command, get_kvs(), get_cluster_state());
            break;
        case Instruction::c_GET:
            instruction_handler::handle_get(connection, command, get_kvs(), get_cluster_state());
            break;
        case Instruction::c_ERASE:
            instruction_handler::handle_erase(connection, command, get_kvs(), get_cluster_state());
            break;
        case Instruction::c_MEET:
            instruction_handler::handle_meet(connection, command, get_cluster_state());
            break;
        case Instruction::c_MIGRATE_SLOT:
            instruction_handler::handle_migrate_slot(connection, command, get_cluster_state());
            break;
        case Instruction::c_IMPORT_SLOT:
            instruction_handler::handle_import_slot(connection, command, get_cluster_state());
            break;
        case Instruction::c_CLUSTER_MIGRATION_FINISHED:
            instruction_handler::handle_migration_finished(command, get_cluster_state());
            break;
        case Instruction::c_GET_SLOTS:
            instruction_handler::handle_get_slots(connection, command, get_cluster_state());
            break;
        default:
            protocol::send_instruction(connection, Status::new_not_supported("Unknown instruction"));
            break;
        }
    }

    void Node::handle_connection(net::Connection& connection) {
        try {
            MetaData meta_data = node::protocol::get_metadata(connection);
            command command = node::protocol::get_command(connection, meta_data.argc, meta_data.command_size);
            execute_instruction(connection, meta_data, command);
        }
        catch (const std::exception& e) {
            return;
        }
    }
}
