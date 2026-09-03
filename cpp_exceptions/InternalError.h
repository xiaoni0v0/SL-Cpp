#pragma once

#include "SLException.h"
#include "SourceLocation.h"

#include <format>
#include <optional>
#include <string>
#include <utility>

// 内部错误：代表编译器/虚拟机自身的实现出了 bug。
//
// **唯一一个永远不转成 SL 异常的宿主异常**——它没有对应的 SL 类，也绝不能被 SL 代码捕获，
// 否则 `eval("...")` 能把编译器自己的 bug 吞掉。所以它虽然也带位置字段，用途只是打给开发者看
class InternalError : public SLException {
    // 运行期（对象模型/GC/虚拟机）出的内部错误没有"出错在源码哪一行"可言，此时为空
    std::optional<SourceLocation> location_;

    static constexpr auto kBlurb =
        "InternalError (This is usually because the compiler/VM itself has a bug, "
        "not a problem with your SL code)";

  public:
    explicit InternalError(const std::string &message)
        : SLException{std::format("{}: {}", kBlurb, message), message} {}

    InternalError(std::string file_path, const int row, const int col, const std::string &message)
        : SLException{
              std::format("{}:{}:{}:\n{}: {}", file_path, row, col, kBlurb, message), message
          },
          location_{SourceLocation{std::move(file_path), row, col}} {}

    // 无位置的那种形态返回空
    [[nodiscard]] const std::optional<SourceLocation> &location() const { return location_; }
};
