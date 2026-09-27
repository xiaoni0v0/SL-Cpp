#pragma once

#include "../common/Position.h"
#include "SLException.h"

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

    InternalError(std::string file_path, const Position pos, const std::string &message)
        : SLException{
              std::format("{}:{}:{}:\n{}: {}", file_path, pos.row, pos.col, kBlurb, message),
              message
          },
          location_{SourceLocation{std::move(file_path), pos}} {}

    [[nodiscard]] const std::optional<SourceLocation> &location() const { return location_; }
};
