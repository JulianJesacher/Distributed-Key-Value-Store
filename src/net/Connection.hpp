#pragma once

#include <optional>
#include <arpa/inet.h>

#include "FileDescriptor.hpp"

namespace net {
    class Connection {
    public:
        Connection(FileDescriptor&& fd, sockaddr_in client);
        explicit Connection(FileDescriptor&& fd);

    private:
        FileDescriptor fd_;
        std::optional<sockaddr_in> client_ = std::nullopt;
    };
}