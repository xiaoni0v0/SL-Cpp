#pragma once

#include "Object.h"

class SyntaxError;
class EncodingError;
class FileNotFoundError;

// 把宿主（C++）层的异常翻成对应的 SL 异常对象

[[nodiscard]] ObjectRef exception_from(const SyntaxError &error);
[[nodiscard]] ObjectRef exception_from(const EncodingError &error);
[[nodiscard]] ObjectRef exception_from(const FileNotFoundError &error);
