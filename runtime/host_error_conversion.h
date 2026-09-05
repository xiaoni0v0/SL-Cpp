#pragma once

#include "Object.h"

#include <string>

class SyntaxError;
class EncodingError;
class FileNotFoundError;

// 把宿主（C++）层的异常翻成对应的 SL 异常对象。这是 .ai/notes/cpp-layer-vs-sl-layer.md 那份
// 分层约定里"某个边界要把编译失败转成 SL 异常对象"的具体落地——现在还没有调用方（`eval`、
// 文件加载都没写呢），先把转换规则钉死。
//
// **`InternalError` 故意没有对应函数，以后也不会有**：它没有 SL 类，也绝不能被 SL 代码捕获，
// 转换这件事对它不成立。
//
// 每个函数按 `.args` 只塞一个字符串——跟 `TypeError("...")` 那个例子一致（见 SL.md 4.2.23）。
// `SyntaxError` 要不要在 `.args` 之外再暴露 file/row/col（类似 CPython 那样），
// 是需要另外拍板的语言设计问题，见 .ai/context.md 悬而未决一节，这里先不越权替它做决定
[[nodiscard]] ObjectRef exception_from(const SyntaxError &error);
[[nodiscard]] ObjectRef exception_from(const EncodingError &error);
[[nodiscard]] ObjectRef exception_from(const FileNotFoundError &error);
