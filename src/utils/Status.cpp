#include "Status.hpp"

#include <utility>

Status::Status() noexcept
{
    errorCode_ = StatusCode::s_Ok;
    errorMsg_ = "";
}

Status::Status(StatusCode code, const ByteArray& msg)
{
    errorCode_ = code;
    errorMsg_ = msg.to_string();
}