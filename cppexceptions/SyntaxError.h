#pragma once

#include "SLException.h"
#include "SourceLocation.h"

#include <format>
#include <string>
#include <utility>

// 语法错误。是唯一一个必定要在 `eval` 边界上转成 SL 异常对象的宿主异常
class SyntaxError : public SLException {
    SourceLocation location_;

  public:
    SyntaxError(std::string file_path, const int row, const int col, const std::string &message)
        : SLException{
              std::format("{}:{}:{}:\nSyntaxError: {}", file_path, row, col, message), message
          },
          location_{std::move(file_path), row, col} {}

    [[nodiscard]] const SourceLocation &location() const { return location_; }
};
