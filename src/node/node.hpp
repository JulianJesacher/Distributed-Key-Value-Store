#include <memory>

#include "../KVS/IKeyValueStore.hpp"
#include "../net/Connection.hpp"
#include "ProtocolHandler.hpp"
#include "Cluster.hpp"

namespace node {

    class Node {
    public:

        Node(key_value_store::IKeyValueStore& kvs): kvs_{ &kvs } {}

        key_value_store::IKeyValueStore& get_kvs() const {
            return *kvs_;
        }

        cluster::ClusterState& get_cluster_state() {
            return cluster_state_;
        }

    private:

        void execute_instruction(net::Connection& connection,
            const protocol::MetaData& meta_data,
            const protocol::command& command);

        void handle_connection(net::Connection& connection);

        std::unique_ptr<key_value_store::IKeyValueStore> kvs_;
        cluster::ClusterState cluster_state_;
    };
}
