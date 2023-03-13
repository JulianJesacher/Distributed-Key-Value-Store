#include "Connection.hpp"

namespace net {

    Connection::Connection(FileDescriptor&& fd, sockaddr_in client) {
        fd_ = std::move(fd);
        client_ = std::make_optional<sockaddr_in>(client);
    }

    Connection::Connection(FileDescriptor&& fd) {
        fd_ = std::move(fd);
    }
}