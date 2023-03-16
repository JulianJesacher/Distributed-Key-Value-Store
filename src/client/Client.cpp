#include "Client.hpp"

namespace client {

    Client::Client(): socket_(std::make_optional<net::Socket>()) {
    }

    net::Connection Client::connect(const std::string& destination, uint16_t port) {
        net::Connection res = socket_.value().connect(destination, port);
        socket_.reset();
        return res;
    }

    net::Connection Client::connect(uint16_t port) {
        return connect("127.0.0.1", port);
    }

}  // namespace client