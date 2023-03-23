#include "ProtocolHandler.hpp"
#include "../KVS/IKeyValueStore.hpp"
#include "../utils/Status.hpp"
#include "Cluster.hpp"

namespace node::instruction_handler {

    void handle_put(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state);

    void handle_get(net::Connection& connection, const protocol::command& command,
        key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state);

    void handle_erase(net::Connection& connection, const protocol::command& command,
        key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state);

    void handle_meet(net::Connection& connection, const protocol::command& command, cluster::ClusterState& cluster_state);
}