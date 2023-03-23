#include <memory>

#include "../KVS/IKeyValueStore.hpp"
#include "../KVS/InMemoryKVS.hpp"
#include "../net/Connection.hpp"
#include "ProtocolHandler.hpp"
#include "Cluster.hpp"

namespace node {

    class Node {
    public:

        Node(std::unique_ptr<key_value_store::IKeyValueStore> kvs, cluster::ClusterState state) {
            kvs_ = std::move(kvs);
            cluster_state_ = state;
        }

        static Node new_in_memory_node(cluster::ClusterState state) {
            return Node{ std::make_unique<key_value_store::InMemoryKVS>(), state };
        }

        key_value_store::IKeyValueStore& get_kvs() const {
            return *kvs_;
        }

        cluster::ClusterState& get_cluster_state() {
            return cluster_state_;
        }

        void execute_instruction(net::Connection& connection,
            const protocol::MetaData& meta_data,
            const protocol::command& command);

        void handle_connection(net::Connection& connection);

        std::unique_ptr<key_value_store::IKeyValueStore> kvs_;
        cluster::ClusterState cluster_state_;
    };
}
