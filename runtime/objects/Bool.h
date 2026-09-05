#pragma once

#include "../Object.h"

/**
 * 有负载的单例：True、False
 */
class Bool final : public Object {
    bool value_;

    SL_HEAP_ONLY;
    Bool(Type *type, bool value);

  public:
    [[nodiscard]] bool value() const { return value_; }
    [[nodiscard]] std::size_t size_bytes() const override { return sizeof(*this); }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
