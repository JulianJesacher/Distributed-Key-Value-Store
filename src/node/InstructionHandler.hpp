#pragma once

#include "ProtocolHandler.hpp"
#include "../KVS/IKeyValueStore.hpp"
#include "../utils/Status.hpp"
#include "Cluster.hpp"

namespace node::instruction_handler {

    void handle_put(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::Command& command, key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state);

    void handle_get(net::Connection& connection, const protocol::Command& command,
        key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state);

    void handle_erase(net::Connection& connection, const protocol::Command& command,
        key_value_store::IKeyValueStore& kvs, cluster::ClusterState& cluster_state);

    void handle_meet(net::Connection& connection, const protocol::Command& command, cluster::ClusterState& cluster_state);

    void handle_migrate_slot(net::Connection& connection, const protocol::Command& command, cluster::ClusterState& cluster_state);

    void handle_import_slot(net::Connection& connection, const protocol::Command& command, cluster::ClusterState& cluster_state);

    void handle_migration_finished(const protocol::Command& command, cluster::ClusterState& cluster_state);

    void handle_get_slots(net::Connection& connection, const protocol::Command& command, cluster::ClusterState& cluster_state);
}