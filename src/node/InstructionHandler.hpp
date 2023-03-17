#include "ProtocolHandler.hpp"
#include "../KVS/IKeyValueStore.hpp"

namespace node::instruction_handler {
    void handle_put(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs);

    void handle_get(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs);

    void handle_erase(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs);
}