#pragma once

#include "../../numeric/BigDec.h"
#include "../Object.h"

// SL 的 decimal 对象。算术全在 BigDec + DecContext 里，这里只负责"是个 SL 对象"
class Decimal final : public Object {
    BigDec value_;

  public:
    explicit Decimal(BigDec value);

    [[nodiscard]] const BigDec &value() const { return value_; }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
