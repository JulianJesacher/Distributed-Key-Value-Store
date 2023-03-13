#pragma once

#include "ByteArray.hpp"

#include <string>

enum class StatusCode
{
    s_Ok = 0,
    s_NotFound = 1,
    s_NotSupported = 2,
    s_InvalidArgument = 3
};

class Status
{
public:
    static Status new_ok() {
        return Status();
    };

    static Status new_not_found(const ByteArray& msg) {
        return Status(StatusCode::s_NotFound, msg);
    }

    static Status new_not_supported(const ByteArray& msg) {
        return Status(StatusCode::s_NotSupported, msg);
    }

    static Status new_invalid_argument(const ByteArray& msg) {
        return Status(StatusCode::s_InvalidArgument, msg);
    }

    bool is_ok() const {
        return errorCode_ == StatusCode::s_Ok;
    }
    bool is_not_found() const {
        return errorCode_ == StatusCode::s_NotFound;
    }
    bool is_not_supported() const {
        return errorCode_ == StatusCode::s_NotSupported;
    }
    bool is_invalid_argument() const {
        return errorCode_ == StatusCode::s_InvalidArgument;
    };

    std::string get_msg() const {
        return errorMsg_;
    }

private:
    Status() noexcept;
    Status(StatusCode code, const ByteArray& msg);

    StatusCode errorCode_;
    std::string errorMsg_;
};