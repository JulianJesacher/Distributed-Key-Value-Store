#include <memory>
#include <unordered_map>
#include <atomic>

#include "../KVS/IKeyValueStore.hpp"
#include "../KVS/InMemoryKVS.hpp"
#include "../net/Connection.hpp"
#include "../net/Epoll.hpp"
#include "ProtocolHandler.hpp"
#include "Cluster.hpp"

namespace node {

    constexpr int NODE_WAIT_TIMEOUT = 1000;

    class Node {
    public:

        Node(int client_port = 3000, int cluster_port = 13000): Node{ std::make_unique<key_value_store::InMemoryKVS>(), cluster::ClusterState(), client_port, cluster_port } {}

        Node(std::unique_ptr<key_value_store::IKeyValueStore> kvs, cluster::ClusterState state, int client_port, int cluster_port);

        //Disable copying and moving
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        static Node new_in_memory_node(cluster::ClusterState state, int client_port = 3000, int cluster_port = 13000) {
            return Node{ std::make_unique<key_value_store::InMemoryKVS>(), state, client_port, cluster_port };
        }

        key_value_store::IKeyValueStore& get_kvs() const {
            return *kvs_;
        }

        cluster::ClusterState& get_cluster_state() {
            return cluster_state_;
        }

        void execute_instruction(net::Connection& connection, const protocol::MetaData& meta_data, const protocol::Command& command);

        void handle_connection(net::Connection& connection);

        void start() {
            running_ = true;
            main_loop();
        }

        void stop() {
            running_ = false;
        }

        net::Epoll& get_connections_epoll() {
            return connections_epoll_;
        }

    private:
        void main_loop();

        std::unique_ptr<key_value_store::IKeyValueStore> kvs_;
        cluster::ClusterState cluster_state_;
        net::Epoll connections_epoll_;
        std::unordered_map<int, net::Connection> fd_to_connection_;

        int client_port_;
        int cluster_port_;
        std::atomic<bool> running_;
    };
}
