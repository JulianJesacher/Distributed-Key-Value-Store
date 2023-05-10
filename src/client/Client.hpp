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

        Status connect_to_node(const std::string& ip, int port);

        void disconnect_all();

        std::unordered_map<std::string, net::Connection>& get_nodes_connections() {
            return nodes_connections_;
        }

        Status put_value(const std::string& key, const ByteArray& value, int offset = 0);
        Status put_value(const std::string& key, const std::string& value, int offset = 0);
        Status put_value(const std::string& key, const char* value, uint64_t size, int offset = 0);

        Status get_value(const std::string& key, ByteArray& value, int offset = 0, int size = 0);

        Status erase_value(const std::string& key);

        std::array<std::string, node::cluster::CLUSTER_AMOUNT_OF_SLOTS>& get_slot_nodes() {
            return slots_nodes_;
        }

        Status get_update_slot_info();

        Status migrate_slot(uint16_t slot, const std::string& importing_ip, int importing_port);

        Status import_slot(uint16_t slot, const std::string& migrating_ip, int migrating_port);

        Status add_node_to_cluster(const std::string& name, const std::string& ip, int client_port, int cluster_port);

    private:

        bool handle_move(node::protocol::Command& received_cmd, uint16_t slot);

        observer_ptr<net::Connection> get_node_connection_by_slot(uint16_t slot_number);

        bool handle_ask(node::protocol::Command& received_cmd);

        bool handle_no_ask_error(node::protocol::Command& received_cmd);

        Status get_value(observer_ptr<net::Connection> link, const std::string& key,
            ByteArray& value, int offset = 0, int size = 0, bool asking = false);

        Status put_value(observer_ptr<net::Connection> link, const std::string& key,
            const char* value, uint64_t size, int offset);

        Status erase_value(observer_ptr<net::Connection> link, const std::string& key, bool asking);

        observer_ptr<net::Connection> get_random_connection();

        void update_slot_info(ByteArray& data);

        Status handle_slot_migration(uint16_t slot, const std::string& partner_ip, int partner_port, node::protocol::Instruction instruction);

        std::array<std::string, node::cluster::CLUSTER_AMOUNT_OF_SLOTS> slots_nodes_;
        std::unordered_map<std::string, net::Connection> nodes_connections_;
    };

}