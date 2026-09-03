#pragma once

#include "SLException.h"

#include <cstddef>
#include <format>
#include <string>
#include <utility>

// 编解码失败。位置是**字节偏移**而不是行列——出错时字节流还没能被切成行，
// 所以不用 SourceLocation
class EncodingError : public SLException {
    std::string file_path_;
    std::size_t offset_{0};

  public:
    EncodingError(std::string file_path, const std::size_t offset, const std::string &message)
        : SLException{
              std::format("{}: offset {}:\nEncodingError: {}", file_path, offset, message), message
          },
          file_path_{std::move(file_path)}, offset_{offset} {}

    [[nodiscard]] const std::string &file_path() const { return file_path_; }
    [[nodiscard]] std::size_t offset() const { return offset_; }
};
