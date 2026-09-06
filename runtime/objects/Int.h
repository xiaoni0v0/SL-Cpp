#pragma once

#include "../../numeric/BigInt.h"
#include "../Object.h"

class Int final : public Object {
    BigInt value_;

    SL_MAKE_REF_ONLY;
    explicit Int(BigInt value);

  public:
    [[nodiscard]] const BigInt &value() const { return value_; }
    [[nodiscard]] std::size_t size_bytes() const override {
        return sizeof(*this) + value_.heap_bytes();
    }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
