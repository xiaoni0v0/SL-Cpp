#pragma once

#include "SLException.h"

#include <format>
#include <string>
#include <utility>

// 文件不存在
class FileNotFoundError : public SLException {
    std::string path_;

  public:
    explicit FileNotFoundError(std::string path)
        : SLException{
              std::format("FileNotFoundError: No such file or directory: \"{}\"", path),
              std::format("No such file or directory: \"{}\"", path)
          },
          path_{std::move(path)} {}

    [[nodiscard]] const std::string &path() const { return path_; }
};
