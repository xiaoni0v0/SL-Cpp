#pragma once

#include "SLException.h"
#include "SourceLocation.h"

#include <format>
#include <optional>
#include <string>
#include <utility>

// 内部错误，唯一一个永远不转成 SL 异常的宿主异常
class InternalError : public SLException {
    std::optional<SourceLocation> location_;

    static constexpr auto kBlurb{
        "InternalError (This is usually because the compiler/VM itself has a bug, "
        "not a problem with your SL code)"
    };

  public:
    explicit InternalError(const std::string &message)
        : SLException{std::format("{}: {}", kBlurb, message), message} {}

    InternalError(std::string file_path, const int row, const int col, const std::string &message)
        : SLException{
              std::format("{}:{}:{}:\n{}: {}", file_path, row, col, kBlurb, message), message
          },
          location_{SourceLocation{std::move(file_path), row, col}} {}

    [[nodiscard]] const std::optional<SourceLocation> &location() const { return location_; }
};
