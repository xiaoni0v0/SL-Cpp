#pragma once

#include "../../numeric/BigDec.h"
#include "../Object.h"

class Decimal final : public Object {
    BigDec value_;

    SL_MAKE_REF_ONLY;
    // 主构造
    Decimal(Type *type, BigDec value);
    // 便利构造
    explicit Decimal(BigDec value);

  public:
    [[nodiscard]] const BigDec &value() const { return value_; }
    [[nodiscard]] std::size_t size_bytes() const override {
        return sizeof(*this) + value_.heap_bytes();
    }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
