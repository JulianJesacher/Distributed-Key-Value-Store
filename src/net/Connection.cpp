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
        fd_ = std::make_shared<FileDescriptor>(std::move(fd));
        client_ = std::make_optional<sockaddr_in>(client);
    }

    Connection::Connection(FileDescriptor&& fd) {
        fd_ = std::make_shared<FileDescriptor>(std::move(fd));
    }

    int Connection::fd() const {
        return  fd_->unwrap();
    }

    bool Connection::is_connected() const {
        return fd_.get() != nullptr && fd_->unwrap() != -1;
    }

    ssize_t Connection::send(const std::string& data) {
        return net::send(fd_->unwrap(), data.c_str(), data.size());
    }

    ssize_t Connection::send(const char* data, uint64_t size) {
        auto sent = net::send(fd_->unwrap(), data, size);
        if (sent != size) {
            throw std::runtime_error("Failed to send all data: " + std::to_string(errno));
        }
        return sent;
    }

    ssize_t Connection::send(std::span<const char> data) {
        return net::send(fd_->unwrap(), data);
    }

    ssize_t Connection::receive_all(std::ostream& stream) const {
        char buf[net::receive_all_buffer_size];
        std::span<char> data(buf, net::receive_all_buffer_size);

        ssize_t total_byte_received = 0;
        while (ssize_t bytes_received = net::receive(fd_->unwrap(), data)) {
            total_byte_received += bytes_received;
            stream.write(buf, net::receive_all_buffer_size);
        }
        return total_byte_received;
    }

    ssize_t Connection::receive(char* data, uint64_t size) const {
        return net::receive(fd_->unwrap(), data, size);
    }
    ssize_t Connection::receive(std::span<char> data) const {
        return net::receive(fd_->unwrap(), data);
    }
}