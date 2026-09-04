#pragma once

#include "../Object.h"
#include "Tuple.h"

#include <vector>

// SL 异常对象。`BaseException` 及其全部子类（`TypeError`/`ValueError`/……）共用这一个 C++ 类——
// 它们在存储层面完全一样，只是 `type()` 不同，跟 `Singleton` 覆盖 `None`/`Ellipsis` 等好几个
// SL 类是同一个道理（见 .ai/notes/object-model-conventions.md）。
//
// 只存 `args_` 一个字段，对应 SL.md 4.2.26 的 `BaseException(*args)`：`.args` 是构造时收到的
// 位置实参元组。这跟 CPython 的实现选择一致——`BaseException.args` 是 C 结构体里的一个槽位，
// 不是走 `__dict__`，只是这里目前还没有通用的属性表机制，`.args` 暂时只能从 C++ 直接取
// （`args()`），等属性协议落地后再接上 `getattr(exc, 'args')` 这条路
class Exception final : public Object {
    Ref<Tuple> args_;

    SL_HEAP_ONLY;
    // type 必须是 BaseException 或其子类；调用方保证，构造时用 assert 兜底（见 .cpp）
    Exception(Type *type, std::vector<ObjectRef> args);

  public:
    [[nodiscard]] Tuple *args() const { return args_.get(); }

  private:
    void visit_own_refs(RefVisitor &visitor) override;
};
