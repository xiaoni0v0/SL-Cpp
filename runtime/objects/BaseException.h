#pragma once

#include "../Object.h"
#include "Tuple.h"

#include <vector>

// SL 异常对象。整棵异常树共用这一个 C++ 类，只是 `type()` 不同
class BaseException final : public Object {
    Ref<Tuple> args_;

    SL_MAKE_REF_ONLY;
    // 调用方保证 type 必须是 SL 层 BaseException 或其子类
    BaseException(Type *type, std::vector<ObjectRef> args);

  public:
    [[nodiscard]] Tuple *args() const { return args_.get(); }

    [[nodiscard]] std::size_t size_bytes() const override { return sizeof(*this); }

  private:
    void visit_own_refs(RefVisitor &visitor) override;
};
