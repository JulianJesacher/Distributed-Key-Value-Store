#include <string>
#include <unordered_map>

#include "../net/Connection.hpp"
#include "../node/Cluster.hpp"
#include "../net/Socket.hpp"
#include "../utils/ByteArray.hpp"
#include "../utils/Status.hpp"
#include "../node/ProtocolHandler.hpp"

namespace client {

    class Client {
    public:

        Client() = default;

        bool connect_to_node(const std::string& ip, int port);

        void disconnect_all();

        std::unordered_map<std::string, net::Connection>& get_nodes_connections() {
            return nodes_connections_;
        }

        Status put_value(const std::string& key, const ByteArray& value, int offset = 0);
        Status put_value(const std::string& key, const std::string& value, int offset = 0);
        Status put_value(const std::string& key, const char* value, uint64_t size, int offset = 0);

        Status get_value(const std::string& key, ByteArray& value, int offset = 0, int size = 0);

    private:

        bool handle_move(node::protocol::Command& received_cmd, uint16_t slot);

        observer_ptr<net::Connection> get_node_connection_by_slot(uint16_t slot_number);

        bool handle_ask(node::protocol::Command& received_cmd);

        bool handle_no_ask_error(node::protocol::Command& received_cmd);

        Status get_value(observer_ptr<net::Connection> link, const std::string& key,
            ByteArray& value, int offset = 0, int size = 0, bool asking = false);

        Status put_value(observer_ptr<net::Connection> link, const std::string& key,
            const char* value, uint64_t size, int offset);

        std::array<std::string, node::cluster::CLUSTER_AMOUNT_OF_SLOTS> slots_nodes_;
        std::unordered_map<std::string, net::Connection> nodes_connections_;
    };

}