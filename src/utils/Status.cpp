#include "Status.hpp"

#include <utility>

Status::Status() noexcept
{
    errorCode_ = StatusCode::s_Ok;
    errorMsg_ = "";
}

Status::Status(StatusCode code, const std::string& msg)
{
    errorCode_ = code;
    errorMsg_ = msg;
}