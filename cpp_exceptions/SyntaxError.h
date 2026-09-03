#pragma once

#include "SLException.h"
#include "SourceLocation.h"

#include <format>
#include <string>
#include <utility>

// 语法/语义错误。是唯一一个**必定**要在 `eval` 边界上转成 SL 异常对象的宿主异常，
// 所以位置三件套必须原样留着
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
