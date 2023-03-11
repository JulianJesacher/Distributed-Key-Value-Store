#include "../include/Status.h"

#include <utility>

Status::Status() noexcept
{
    errorCode_ = StatusCode::s_Ok;
    errorMsg_ = "";
}

Status::Status(StatusCode code, const ByteArray& msg)
{
    errorCode_ = code;
    errorMsg_ = msg.toString();
}

Status::Status(const Status& other)
{
    errorCode_ = other.errorCode_;
    errorMsg_ = other.errorMsg_;
}

Status& Status::operator=(const Status& other)
{
    errorCode_ = other.errorCode_;
    errorMsg_ = other.errorMsg_;
    return *this;
}

Status::Status(Status&& other) noexcept
{
    errorCode_ = other.errorCode_;
    errorMsg_ = std::move(other.errorMsg_);
}

Status& Status::operator=(Status&& other) noexcept
{
    errorCode_ = other.errorCode_;
    errorMsg_ = std::move(other.errorMsg_);
    return *this;
}