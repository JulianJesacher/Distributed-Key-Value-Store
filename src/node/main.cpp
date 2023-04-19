#include "node.hpp"

int main(int argc, char** argv) {
    uint16_t client_port = 5000, cluster_port = 5001;
    node::Node node = node::Node::new_in_memory_node("node", client_port, cluster_port, "127.0.0.1");
    node.start();
    return 0;
}
