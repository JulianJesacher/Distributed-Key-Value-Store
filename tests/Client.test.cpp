#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <thread>
#include <chrono>
#include <sys/epoll.h>

#include "client/Client.hpp"
#include "node/node.hpp"
#include "net/Socket.hpp"
#include "net/Connection.hpp"

using namespace client;
using namespace node;
using namespace std::chrono_literals;

TEST_CASE("Test connect") {
    int client_port = 8080, cluster_port = 8081;
    Client client{};
    Node node{ client_port, cluster_port };

    auto thread = std::thread([&node]() {
        node.start();
        });
    std::this_thread::sleep_for(100ms);

    SUBCASE("No connections open") {
        //Initially, no connections are open
        CHECK(node.get_connections_epoll().wait(0) == 0);
        CHECK_EQ(client.get_nodes_connections().size(), 0);
    }

    SUBCASE("Client connect") {
        //After connecting to a node, a connection should be open
        client.connect_to_node("127.0.0.1", client_port);
        CHECK(node.get_connections_epoll().wait(0) == 1);
        CHECK_EQ(client.get_nodes_connections().size(), 1);
    }

    SUBCASE("Socket connect") {
        //Also connections from a socket should be accepted
        net::Socket client_socket{};
        client_socket.connect(client_port);
        CHECK(node.get_connections_epoll().wait(0) == 1);
    }

    node.stop();
    thread.join();
}