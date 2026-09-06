#pragma once

#include "../Object.h"

#include <string>

class Str final : public Object {
    std::u32string value_; // 底层是 u32string

    SL_MAKE_REF_ONLY;
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
