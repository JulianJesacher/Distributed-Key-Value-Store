#include "FileDescriptor.hpp"

#include <unistd.h>

namespace net {

    FileDescriptor::FileDescriptor(int fd) {
        fd_ = std::make_optional<int>(fd);
    }

    FileDescriptor::~FileDescriptor() {
        if (fd_.has_value()) {
            close(fd_.value());
        }
    }

    FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept {
        fd_ = std::move(other.fd_);
        other.fd_.reset();
    }

    FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        other.fd_ = std::move(fd_);
        other.fd_.reset();
        return *this;
    }

    int FileDescriptor::unwrap() const {
        return fd_.value_or(-1);
    }

}