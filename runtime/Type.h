#pragma once

#include "Object.h"

#include <string>
#include <vector>

// SL 的类型对象
class Type final : public Object {
    std::string name_;
    std::vector<Ref<Type>> bases_;
    std::vector<Type *> mro_; // 第 0 项是自己。存裸指针、不是强引用

    SL_MAKE_REF_ONLY;
    Type(Type *meta, std::string name, std::vector<Ref<Type>> bases);

    void visit_own_refs(RefVisitor &visitor) override;

  public:
    [[nodiscard]] const std::string &name() const { return name_; }
    [[nodiscard]] const std::vector<Ref<Type>> &bases() const { return bases_; }
    [[nodiscard]] const std::vector<Type *> &mro() const { return mro_; }

    [[nodiscard]] bool is_subtype_of(const Type *other) const;
    [[nodiscard]] std::size_t size_bytes() const override;
};
