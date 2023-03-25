#include "Epoll.hpp"

namespace net {

    Epoll::Epoll(int max_events) {
        int fd = epoll_create1(0);
        epoll_fd_ = FileDescriptor(fd);
        max_events_ = max_events;
        events_.resize(max_events);
    }

    void Epoll::add_event(FileDescriptor& fd, uint32_t events) {
        add_event(fd.unwrap());
    }

    void Epoll::add_event(int fd, uint32_t events) {
        epoll_event event;
        event.data.fd = fd;
        event.events = events;

        epoll_ctl(epoll_fd_.unwrap(), EPOLL_CTL_ADD, fd, &event);
    }

    void Epoll::remove(FileDescriptor& fd) {
        epoll_ctl(epoll_fd_.unwrap(), EPOLL_CTL_DEL, fd.unwrap(), nullptr);
    }

    void Epoll::reset_occurred_events() {
        wait(0);
    }

    int Epoll::wait(int timeout) {
        return epoll_wait(epoll_fd_.unwrap(), events_.data(), max_events_, timeout);
    }

    std::vector<epoll_event> Epoll::events() {
        return events_;
    }
}