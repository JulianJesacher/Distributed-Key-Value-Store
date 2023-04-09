#pragma once

#include <sys/epoll.h>
#include <cstdint>
#include <vector>

#include "FileDescriptor.hpp"

namespace net {
    class Epoll {

    public:
        Epoll(int max_events = 10);
        ~Epoll() = default;

        void add_event(int fd, uint32_t events = EPOLLIN | EPOLLET);
        void add_event(FileDescriptor& fd, uint32_t events = EPOLLIN | EPOLLET);
        void remove(FileDescriptor& fd);
        void reset_occurred_events();

        [[nodiscard]] int wait(int timeout = -1);
        [[nodiscard]] std::vector<epoll_event> get_events();
        [[nodiscard]] int get_event_fd(int index) const;
        [[nodiscard]] int get_epoll_fd() const;

    private:
        FileDescriptor epoll_fd_;
        int max_events_;
        std::vector<epoll_event> events_;
    };
}