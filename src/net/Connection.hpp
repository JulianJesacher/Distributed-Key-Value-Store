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

    constexpr int receive_all_buffer_size = 256;

    class Connection {
    public:
        Connection() = default;
        Connection(FileDescriptor&& fd, sockaddr_in client);
        explicit Connection(FileDescriptor&& fd);

        int fd() const;
        bool is_connected() const;

        ssize_t send(const std::string& data);
        ssize_t send(const char* data, uint64_t size);
        ssize_t send(std::span<const char> data);

        ssize_t receive_all(std::ostream& stream) const;
        ssize_t receive(char* data, uint64_t size) const;
        ssize_t receive(std::span<char> data) const;

    private:
        FileDescriptor fd_;
        std::optional<sockaddr_in> client_ = std::nullopt;
    };
}