#include "host_error_conversion.h"

#include "../cppexceptions/EncodingError.h"
#include "../cppexceptions/FileNotFoundError.h"
#include "../cppexceptions/SyntaxError.h"
#include "Runtime.h"
#include "objects/BaseException.h"
#include "objects/Str.h"

#include <string>
#include <vector>

namespace {
ObjectRef single_arg_exception(Type *const type, const std::string &message) {
    return ObjectRef{
        make_ref<BaseException>(type, std::vector{ObjectRef{Str::from_utf8(message)}})
    };
}
} // namespace

ObjectRef exception_from(const SyntaxError &error) {
    return single_arg_exception(Runtime::type_syntax_error(), error.message());
}

ObjectRef exception_from(const EncodingError &error) {
    // 模块建好后再改这一行
    return single_arg_exception(Runtime::type_io_error(), error.message());
}

ObjectRef exception_from(const FileNotFoundError &error) {
    return single_arg_exception(Runtime::type_io_error(), error.message());
}
