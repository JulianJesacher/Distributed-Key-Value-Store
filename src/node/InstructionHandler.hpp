#include "ProtocolHandler.hpp"
#include "../KVS/IKeyValueStore.hpp"
#include "../utils/Status.hpp"

namespace node::instruction_handler {

    Status check_argc(const protocol::command& command, protocol::Instruction instruction);

    void handle_put(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs);

    void handle_get(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs);

    void handle_erase(net::Connection& connection, const protocol::MetaData& metadata,
        const protocol::command& command, key_value_store::IKeyValueStore& kvs);
}