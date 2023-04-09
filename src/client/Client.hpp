#include <string>
#include <unordered_map>

#include "../net/Connection.hpp"
#include "../node/Cluster.hpp"
#include "../net/Socket.hpp"

namespace client {

    class Client {
    public:

        Client() = default;

        void connect_to_node(const std::string& ip, int port);

        void disconnect_all();

        std::unordered_map<std::string, net::Connection>& get_nodes_connections() {
            return nodes_connections_;
        }

    private:
        std::array<std::string, node::cluster::CLUSTER_AMOUNT_OF_SLOTS> slots_nodes_;
        std::unordered_map<std::string, net::Connection> nodes_connections_;
    };

}