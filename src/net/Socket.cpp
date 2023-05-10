#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cerrno>
#include <fcntl.h>

#include "Socket.hpp"

namespace net {

    [[nodiscard]] bool is_listening(int fd) {
        int ret_val;
        socklen_t len = sizeof(ret_val);
        return getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &ret_val, &len) != -1 && ret_val != 0;
    }
    [[nodiscard]] bool is_listening(FileDescriptor& fd) {
        return is_listening(fd.unwrap());
    }

    Socket::Socket() {
        fd_ = FileDescriptor(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (fd_.unwrap() < 0) {
            throw std::runtime_error("error creating socket: " + std::to_string(errno));
        }

        //Set socket option to bind to the same port after terminating without waiting
        int true_flag = 1;
        if (setsockopt(fd_.unwrap(), SOL_SOCKET, SO_REUSEADDR, &true_flag, sizeof(true_flag))) {
            throw std::runtime_error("error setting SO_REUSEADDR for socket: " + std::to_string(errno));
        }
        if (setsockopt(fd_.unwrap(), SOL_SOCKET, SO_REUSEPORT, &true_flag, sizeof(true_flag))) {
            throw std::runtime_error("error setting SO_REUSEPORT for socket: " + std::to_string(errno));
        }
    }

    void Socket::listen(uint16_t port, int queue_size) const {
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(INADDR_ANY);
        server.sin_port = htons(port);

        if (::bind(fd_.unwrap(), reinterpret_cast<sockaddr*>(&server), sizeof(server))) {
            throw std::runtime_error("failed to bind to server socket: " + std::to_string(errno));
        }
        if (::listen(fd_.unwrap(), queue_size)) {
            throw std::runtime_error("failed to listen: " + std::to_string(errno));
        }
    }

    bool Socket::set_non_blocking() const {
        int flags = fcntl(fd_.unwrap(), F_GETFL, 0);
        if (flags == -1) {
            return false;
        }
        flags |= O_NONBLOCK;
        return fcntl(fd_.unwrap(), F_SETFL, flags) == 0;
    }

    bool Socket::set_keep_alive() const {
        int true_flag = 1;
        return setsockopt(fd_.unwrap(), SOL_SOCKET, SO_KEEPALIVE, &true_flag, sizeof(true_flag)) == 0;
    }

    Connection Socket::accept() const {
        if (!is_listening(fd_.unwrap())) {
            throw std::runtime_error("socket needs to listen before accepting connections");
        }

        sockaddr_in client{};
        socklen_t len{ sizeof(client) };

        FileDescriptor client_fd = FileDescriptor{ ::accept(fd_.unwrap(), reinterpret_cast<sockaddr*>(&client), &len) };
        if (client_fd.unwrap() < 0) {
            throw std::runtime_error("failed to accept connection: " + std::to_string(errno));
        }

        return Connection{ std::move(client_fd), client };
    }

    Connection Socket::connect(const std::string& addr, uint16_t port) {
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(addr.c_str());
        server.sin_port = htons(port);

        if (::connect(fd_.unwrap(), reinterpret_cast<sockaddr*>(&server), sizeof(server))) {
            throw std::runtime_error("failed to connect to server: " + std::to_string(errno));
        }

        return Connection{ std::move(fd_) };
    }

    Connection Socket::connect(uint16_t port) {
        return connect("127.0.0.1", port);
    }

    int Socket::fd() const {
        return fd_.unwrap();
    }
}