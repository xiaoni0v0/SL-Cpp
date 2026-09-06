#pragma once

#include "../Object.h"

class Bool final : public Object {
    bool value_;

    SL_MAKE_REF_ONLY;
    Bool(Type *type, bool value);

  public:
    [[nodiscard]] bool value() const { return value_; }
    [[nodiscard]] std::size_t size_bytes() const override { return sizeof(*this); }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
