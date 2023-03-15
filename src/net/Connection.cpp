#include <sys/socket.h>

#include "Connection.hpp"

namespace net {

    ssize_t send(int fd, std::span<const char> data) {
        return ::send(fd, data.data(), data.size_bytes(), 0);
    }
    ssize_t send(int fd, const char* data, uint64_t size) {
        return send(fd, std::span<const char>(data, size));
    };

    ssize_t receive(int fd, std::span<char> buf) {
        return ::recv(fd, buf.data(), buf.size_bytes(), 0);
    }
    ssize_t receive(int fd, char* buf, uint64_t size) {
        return receive(fd, std::span<char>(buf, size));
    }

    Connection::Connection(FileDescriptor&& fd, sockaddr_in client) {
        fd_ = std::move(fd);
        client_ = std::make_optional<sockaddr_in>(client);
    }

    Connection::Connection(FileDescriptor&& fd) {
        fd_ = std::move(fd);
    }

    int Connection::fd() const {
        return fd_.unwrap();
    }

    ssize_t Connection::send(const std::string& data) {
        return net::send(fd_.unwrap(), data.c_str(), data.size());
    }

    ssize_t Connection::send(const char* data, uint64_t size) {
        return net::send(fd_.unwrap(), data, size);
    }

    ssize_t Connection::receive(std::ostream& stream) const {
        char buf[128];
        std::span<char> data(buf, 128);

        ssize_t bytes_received = net::receive(fd_.unwrap(), data);
        auto it = data.begin();
        for (ssize_t i = 0; i < bytes_received; i++) {
            stream << (*it++);
        }
        return bytes_received;
    }
}