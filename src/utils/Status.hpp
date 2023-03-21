#pragma once

#include "ByteArray.hpp"

#include <string>

enum class StatusCode
{
    s_OK = 0,
    s_NOT_FOUND = 1,
    s_NOT_SUPPORTED = 2,
    s_INVALID_ARGUMENT = 3,
    s_NOT_ENOUGH_MEMORY = 4,
    s_ERROR = 5
};

class Status
{
public:
    static Status new_ok() {
        return Status();
    };

    static Status new_not_found(const std::string& msg) {
        return Status(StatusCode::s_NOT_FOUND, msg);
    }

    static Status new_not_supported(const std::string& msg) {
        return Status(StatusCode::s_NOT_SUPPORTED, msg);
    }

    static Status new_invalid_argument(const std::string& msg) {
        return Status(StatusCode::s_INVALID_ARGUMENT, msg);
    }

    static Status new_not_enough_memory(const std::string& msg) {
        return Status(StatusCode::s_NOT_ENOUGH_MEMORY, msg);
    }

    static Status new_error(const std::string& msg) {
        return Status(StatusCode::s_ERROR, msg);
    }

    bool is_ok() const {
        return errorCode_ == StatusCode::s_OK;
    }
    bool is_not_found() const {
        return errorCode_ == StatusCode::s_NOT_FOUND;
    }
    bool is_not_supported() const {
        return errorCode_ == StatusCode::s_NOT_SUPPORTED;
    }
    bool is_invalid_argument() const {
        return errorCode_ == StatusCode::s_INVALID_ARGUMENT;
    };
    bool is_not_enough_memory() const {
        return errorCode_ == StatusCode::s_NOT_ENOUGH_MEMORY;
    }
    bool is_error() const {
        return errorCode_ == StatusCode::s_ERROR;
    }
    std::string get_msg() const {
        return errorMsg_;
    }

private:
    Status() noexcept;
    Status(StatusCode code, const std::string& msg);

    StatusCode errorCode_;
    std::string errorMsg_;
};