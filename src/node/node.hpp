#include <memory>

#include "../KVS/IKeyValueStore.hpp"
#include "../net/Connection.hpp"
#include "ProtocolHandler.hpp"

namespace node {

    class Node {
    public:

        key_value_store::IKeyValueStore& get_kvs() const {
            return *kvs_;
        }

    private:
    
        void execute_instruction(net::Connection& connection,
            const protocol::MetaData& meta_data,
            const protocol::command& command);

        void handle_connection(net::Connection& connection);

        std::unique_ptr<key_value_store::IKeyValueStore> kvs_;
    };
}
