#pragma once

#include "../../numeric/BigInt.h"
#include "../Object.h"

class Int final : public Object {
    BigInt value_;

    SL_MAKE_REF_ONLY;
    // 主构造
    Int(Type *type, BigInt value);
    // 便利构造
    explicit Int(BigInt value);

  public:
    [[nodiscard]] const BigInt &value() const { return value_; }
    [[nodiscard]] std::size_t size_bytes() const override {
        return sizeof(*this) + value_.heap_bytes();
    }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
