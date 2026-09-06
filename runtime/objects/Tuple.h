#pragma once

#include "../../utils/memory_utils.h"
#include "../Object.h"

#include <vector>

class Tuple final : public Object {
    std::vector<ObjectRef> items_;

    SL_MAKE_REF_ONLY;
    // 主构造
    Tuple(Type *type, std::vector<ObjectRef> items);
    // 便利构造
    explicit Tuple(std::vector<ObjectRef> items);

  public:
    [[nodiscard]] std::size_t size() const { return items_.size(); }
    [[nodiscard]] const std::vector<ObjectRef> &items() const { return items_; }
    // 调用方保证 index < size()
    [[nodiscard]] Object *at(std::size_t index) const;

    [[nodiscard]] std::size_t size_bytes() const override {
        return sizeof(*this) + mem::heap_bytes(items_);
    }

  private:
    void visit_own_refs(RefVisitor &visitor) override;
};
