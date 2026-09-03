#pragma once

#include "Object.h"

#include <string>
#include <vector>

// SL 的类型对象。SL 只有 type 一个元类（SL.md 4.2.2），所以每个 Type 的 type() 都是 type 自己。
//
// 这个类是 final 的：SL 层的用户自定义类是 Type 的**实例**，不是它的 C++ 子类
class Type final : public Object {
    std::string name_;
    std::vector<Ref<Type>> bases_;
    // 方法解析顺序，第 0 项是自己。存裸指针、不是强引用：mro_[0] == this，
    // 强引用会让每个类都自成一个环、引用计数永远归不了零。安全性由 bases_ 兜着——
    // mro_ 里的每个类都能沿 bases_ 链到达，本来就被强引用，GC 顺着 bases_ 也扫得到
    std::vector<Type *> mro_;

  public:
    // meta 是元类（恒为 type）。只有 bootstrap 建立 object/type 这两个互相引用的类时
    // 才允许传 nullptr，之后立刻回填，见 Runtime
    Type(Type *meta, std::string name, std::vector<Ref<Type>> bases);

    [[nodiscard]] const std::string &name() const { return name_; }
    [[nodiscard]] const std::vector<Ref<Type>> &bases() const { return bases_; }
    [[nodiscard]] const std::vector<Type *> &mro() const { return mro_; }

    // 自己也算自己的子类，即 isinstance 的类那一半。调用方保证 other 非空
    [[nodiscard]] bool is_subtype_of(const Type *other) const;

  protected:
    void visit_own_refs(RefVisitor &visitor) override;
};
