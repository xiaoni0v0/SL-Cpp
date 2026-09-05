#include "HostErrorConversion.h"

#include "../cpp_exceptions/EncodingError.h"
#include "../cpp_exceptions/FileNotFoundError.h"
#include "../cpp_exceptions/SyntaxError.h"
#include "Runtime.h"
#include "objects/Exception.h"
#include "objects/Str.h"

#include <string>
#include <vector>

namespace {
ObjectRef single_arg_exception(Type *const type, const std::string &message) {
    return ObjectRef{
        make_ref<Exception>(type, std::vector<ObjectRef>{ObjectRef{Str::from_utf8(message)}})
    };
}
} // namespace

ObjectRef exception_from(const SyntaxError &error) {
    return single_arg_exception(Runtime::syntax_error_type(), error.message());
}

ObjectRef exception_from(const EncodingError &error) {
    // EncodingError 在 SL 层是 exceptions.EncodingError（IOError 的子类，SL.md 4.4.3），
    // 那个模块还没有落脚处，暂时先落到 IOError——比彻底不转好，模块建好后再改这一行
    return single_arg_exception(Runtime::io_error_type(), error.message());
}

ObjectRef exception_from(const FileNotFoundError &error) {
    return single_arg_exception(Runtime::io_error_type(), error.message());
}
