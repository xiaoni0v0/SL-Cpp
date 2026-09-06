#pragma once

#include <stdexcept>
#include <string>
#include <utility>

// 宿主（C++）层异常的根
class SLException : public std::runtime_error {
    std::string message_;

  protected:
    // rendered 是给 what() 的完整呈现；message 是未经装饰的那句话
    SLException(const std::string &rendered, std::string message)
        : std::runtime_error{rendered}, message_{std::move(message)} {}

  public:
    [[nodiscard]] const std::string &message() const { return message_; }
};
