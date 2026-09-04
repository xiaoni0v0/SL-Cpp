#pragma once

#include "../Object.h"

#include <cstddef>
#include <vector>

// SL 的 tuple 对象。不可变指的是这些**引用关系**不可变，不蕴含被引用的对象自己不可变
// （SL.md 4.2.9）
class Tuple final : public Object {
    std::vector<ObjectRef> items_;

    SL_HEAP_ONLY;
    explicit Tuple(std::vector<ObjectRef> items);

  public:
    [[nodiscard]] std::size_t size() const { return items_.size(); }
    [[nodiscard]] const std::vector<ObjectRef> &items() const { return items_; }
    // 调用方保证 index < size()
    [[nodiscard]] Object *at(std::size_t index) const;

  private:
    void visit_own_refs(RefVisitor &visitor) override;
};
