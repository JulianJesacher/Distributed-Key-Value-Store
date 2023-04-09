#pragma once

#include <cstdint>
#include <string>

#include "FileDescriptor.hpp"
#include "Connection.hpp"

namespace net {

    [[nodiscard]] bool is_listening(int fd);

    [[nodiscard]] bool is_listening(FileDescriptor& fd);

    class Socket {
    public:
        Socket();
        ~Socket() = default;

        void listen(uint16_t port, int queue_size = 50) const;

        bool set_non_blocking() const;

        Connection accept() const;

        Connection connect(const std::string &addr, uint16_t port);

        //Connect to localhost
        Connection connect(uint16_t port);

        int fd() const;

    private:
        FileDescriptor fd_;
    };
}