#pragma once

#include "../Object.h"

#include <string>

// 无负载的单例：None、Ellipsis、NotImplemented、StopIteration。
// 它们彼此的差别只有"类型是谁"和"打印成什么"，一个字段都不额外需要，所以共用一个 C++ 类，
// 不为每个单例各开一个空类。每个实例由 Runtime 在 bootstrap 时建立且只建一个，
// 注意类型并不统一：None 的类型是 NoneType，另外三个才是 SingletonType（SL.md 4.2.17）
class Singleton final : public Object {
    std::string name_;

  public:
    Singleton(Type *type, std::string name);

    // 这四个单例在 SL 里都是靠名字取到的（`None`/`Ellipsis`/`NotImplemented`/`StopIteration`），
    // 名字就是它们全部的内容
    [[nodiscard]] const std::string &name() const { return name_; }

  private:
    void visit_own_refs(RefVisitor &) override {}
};

// True / False。有真实负载——真值本身，以及参与算术时按 1/0 折算（SL.md 4.2.5），
// 所以不跟上面那些并进 Singleton
class Bool final : public Object {
    bool value_;

  public:
    explicit Bool(bool value);

    [[nodiscard]] bool value() const { return value_; }

  private:
    void visit_own_refs(RefVisitor &) override {}
};
