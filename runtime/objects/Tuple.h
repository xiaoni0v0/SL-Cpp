#pragma once

#include "../Object.h"

#include <cstddef>
#include <vector>

class Tuple final : public Object {
    std::vector<ObjectRef> items_;

    SL_MAKE_REF_ONLY;
    explicit Tuple(std::vector<ObjectRef> items);

  public:
    [[nodiscard]] std::size_t size() const { return items_.size(); }
    [[nodiscard]] const std::vector<ObjectRef> &items() const { return items_; }
    // 调用方保证 index < size()
    [[nodiscard]] Object *at(std::size_t index) const;

    [[nodiscard]] std::size_t size_bytes() const override {
        return sizeof(*this) + items_.capacity() * sizeof(ObjectRef);
    }

  private:
    void visit_own_refs(RefVisitor &visitor) override;
};
