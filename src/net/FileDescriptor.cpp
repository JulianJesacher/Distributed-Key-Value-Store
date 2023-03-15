#include "FileDescriptor.hpp"

#include <unistd.h>

namespace net {

    FileDescriptor::FileDescriptor(int fd) {
        fd_ = std::make_optional<int>(fd);
    }

    FileDescriptor::~FileDescriptor() {
        if (fd_.has_value() && *fd_ >= 0) {
            close(fd_.value());
        }
    }

    FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept {
        fd_ = std::move(other.fd_);
        other.fd_ = std::nullopt;
    }

    FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        fd_ = std::move(other.fd_);
        other.fd_ = std::nullopt;
        return *this;
    }

    int FileDescriptor::unwrap() const {
        if (fd_.has_value()) {
            return *fd_;
        }
        return -1;
    }

    FileDescriptor::operator int() const {
        return unwrap();
    }

}