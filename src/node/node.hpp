#include <memory>
#include <unordered_map>
#include <atomic>
#include <string>
#include <thread>

#include "../KVS/IKeyValueStore.hpp"
#include "../KVS/InMemoryKVS.hpp"
#include "../net/Connection.hpp"
#include "../net/Epoll.hpp"
#include "ProtocolHandler.hpp"
#include "Cluster.hpp"

namespace node {

    constexpr int NODE_WAIT_TIMEOUT = 1000;
    constexpr int NODE_PING_PAUSE = 1000;

    class Node {
    public:

        ~Node();

        //Disable copying and moving
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        static Node new_in_memory_node(std::string name, uint16_t client_port, uint16_t cluster_port, std::string ip);

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
            gossiping_ = true;
            main_loop();
            gossip_thread_ = std::thread(&Node::gossip, this);
        }

        void stop() {
            running_ = false;
            gossiping_ = false;
            if (gossip_thread_.joinable()) {
                gossip_thread_.join(); //TODO: Detach or join?
            }
        }

        net::Epoll& get_connections_epoll() {
            return connections_epoll_;
        }

        void set_cluster_state(cluster::ClusterState cluster_state) {
            cluster_state_ = cluster_state;
        }

    private:
        Node(std::unique_ptr<key_value_store::IKeyValueStore> kvs,
            uint16_t client_port,
            uint16_t cluster_port,
            std::array<char, cluster::CLUSTER_NAME_LEN> name,
            std::array<char, cluster::CLUSTER_IP_LEN> ip);

        void main_loop();

        void gossip();

        void disconnect(net::Connection& connection);

        std::unique_ptr<key_value_store::IKeyValueStore> kvs_;
        cluster::ClusterState cluster_state_;
        net::Epoll connections_epoll_;
        std::unordered_map<int, net::Connection> fd_to_connection_;

        uint16_t client_port_;
        uint16_t cluster_port_;
        std::atomic<bool> running_;
        std::atomic<bool> gossiping_;
        std::thread gossip_thread_;
        std::array<char, cluster::CLUSTER_NAME_LEN> name_;
        std::array<char, cluster::CLUSTER_IP_LEN> ip_;
    };
}
