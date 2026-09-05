#pragma once

#include "../Object.h"

#include <string>

/**
 * 无负载、只靠一个显示名区分的单例：None、Ellipsis、NotImplemented、StopIteration
 */
class NamedSingleton final : public Object {
    std::string name_;

    SL_HEAP_ONLY;
    NamedSingleton(Type *type, std::string name);

  public:
    [[nodiscard]] const std::string &name() const { return name_; }
    [[nodiscard]] std::size_t size_bytes() const override {
        return sizeof(*this) + name_.capacity();
    }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
