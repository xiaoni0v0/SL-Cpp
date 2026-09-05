#pragma once

#include "../Object.h"

#include <string>

// SL 的 str 对象。不可变，按 Unicode 码点分割（SL.md 4.2.7），所以底层是 u32string
// 而不是 UTF-8 的 std::string——按码点取下标/求长度是 O(1)，代价是转进转出要编解码
class Str final : public Object {
    std::u32string value_;

    SL_HEAP_ONLY;
    explicit Str(std::u32string value);

  public:
    [[nodiscard]] const std::u32string &value() const { return value_; }
    // 码点数，不是字节数
    [[nodiscard]] std::size_t size() const { return value_.size(); }

    [[nodiscard]] static Ref<Str> from_utf8(const std::string &utf8);
    [[nodiscard]] std::string to_utf8() const;

    [[nodiscard]] std::size_t size_bytes() const override {
        return sizeof(*this) + value_.capacity() * sizeof(char32_t);
    }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
