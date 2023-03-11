#pragma once

#include "ByteArray.h"

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
    ~Status() = default;

    Status(const Status& other);
    Status& operator=(const Status& other);

    Status(Status&& other) noexcept;
    Status& operator=(Status&& other) noexcept;

    static Status OK() {
        return Status();
    };

    static Status NotFound(const ByteArray& msg) {
        return Status(StatusCode::s_NotFound, msg);
    }

    static Status NotSupported(const ByteArray& msg) {
        return Status(StatusCode::s_NotSupported, msg);
    }

    static Status InvalidArgument(const ByteArray& msg) {
        return Status(StatusCode::s_InvalidArgument, msg);
    }

    bool ok() const {
        return errorCode_ == StatusCode::s_Ok;
    }
    bool isNotFound() const {
        return errorCode_ == StatusCode::s_NotFound;
    }
    bool isNotSupported() const {
        return errorCode_ == StatusCode::s_NotSupported;
    }
    bool isInvalidArgument() const {
        return errorCode_ == StatusCode::s_InvalidArgument;
    };

    std::string getMsg() const {
        return errorMsg_;
    }

private:
    Status() noexcept;
    Status(StatusCode code, const ByteArray& msg);

    StatusCode errorCode_;
    std::string errorMsg_;
};