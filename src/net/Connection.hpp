#pragma once

#include <optional>
#include <arpa/inet.h>
#include <span>
#include <string>
#include <istream>

#include "FileDescriptor.hpp"

namespace net {

    ssize_t send(int fd, std::span<const char> data);
    ssize_t send(int fd, const char* data, uint64_t size);

    ssize_t receive(int fd, std::span<char> buf);
    ssize_t receive(int fd, char* buf, uint64_t size);

    class Connection {
    public:
        Connection(FileDescriptor&& fd, sockaddr_in client);
        explicit Connection(FileDescriptor&& fd);

        int fd() const;

        ssize_t send(const std::string& data);
        ssize_t send(const char* data, uint64_t size);

        ssize_t receive(std::ostream& stream) const;

    private:
        FileDescriptor fd_;
        std::optional<sockaddr_in> client_ = std::nullopt;
    };
}