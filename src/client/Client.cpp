#include "Client.hpp"

namespace client {

    void Client::connect_to_node(const std::string& ip, int port) {
        net::Socket socket{};
        std::string ip_port = ip + ":" + std::to_string(port);
        nodes_connections_.emplace(ip_port, socket.connect(ip, port));
    }

    void Client::disconnect_all() {
        nodes_connections_.clear();
    }

}  // namespace client