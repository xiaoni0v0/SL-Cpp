#pragma once

#include "../../numeric/BigInt.h"
#include "../Object.h"

// SL 的 int 对象。值语义那部分全在 BigInt 里，这里只负责"是个 SL 对象"
class Int final : public Object {
    BigInt value_;

    SL_HEAP_ONLY;
    explicit Int(BigInt value);

  public:
    [[nodiscard]] const BigInt &value() const { return value_; }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
