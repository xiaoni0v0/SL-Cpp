#pragma once

#include "SLException.h"

#include <format>
#include <string>

// 内部错误：代表编译器自身的实现出了 bug
// 是 exceptions 里唯一没有对应 SL 类的异常，当然更不能在 SL 层捕获
class InternalError : public SLException {
public:
    explicit InternalError(const std::string &file_path, const int row, const int col, const std::string &message)
        : SLException{std::format(
            "{}:{}:{}:\nInternalError (this is a compiler bug, not a problem with your SL code): {}",
            file_path, row, col, message)} {
    }
};
