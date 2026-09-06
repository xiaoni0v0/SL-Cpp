#pragma once

#include <stdexcept>
#include <string>
#include <utility>

// 宿主（C++）层异常的根。
//
// **信息以字段为准，`what()` 只是其中一种渲染。** 这些异常大多要在某个边界上被转成 SL 层的异常
// 对象（`eval` 里的语法错误、读文件失败等），转换方需要的是 file/row/col/message 这些**分开的**
// 值；只留一个拼好的字符串，就等于逼转换方去反解析自己刚拼出来的东西。
//
// `what()` 仍然照常可用（`std::runtime_error` 要求，主循环兜底打印也要），只是它在构造时由字段
// 渲染而来，不是真相本身
class SLException : public std::runtime_error {
    std::string message_;

  protected:
    // rendered 是给 what() 的完整呈现；message 是未经装饰的那句话
    SLException(const std::string &rendered, std::string message)
        : std::runtime_error{rendered}, message_{std::move(message)} {}

  public:
    // 不带位置前缀、不带异常类名的原始信息
    [[nodiscard]] const std::string &message() const { return message_; }
};
