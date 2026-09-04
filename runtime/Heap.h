#pragma once

#include "Object.h"

/**
 * 能提供引用根的东西。实现它并注册进 Heap。
 *
 * 现在只有 Runtime（内置类型 + 单例）；以后还有帧栈、模块表、每份 Code 的常量表。
 */
class GcRootSource {
  public:
    GcRootSource() = default;
    GcRootSource(const GcRootSource &) = delete;
    GcRootSource &operator=(const GcRootSource &) = delete;
    virtual ~GcRootSource() = default;

    // 遍历持有的每个强引用
    virtual void visit_roots(RefVisitor &visitor) = 0;
};

/**
 * 堆，串起全部存活对象，并在需要时做一轮 STW 标记清扫。
 */
class Heap {
    friend class Object;

    // 标记阶段和清理阶段的两个访问者
    class Marker;
    class Clearer;

    // 进出全堆链表
    static void link(Object *obj);
    static void unlink(Object *obj);

  public:
    Heap() = delete;

    // 引用根的管理
    static void add_root_source(GcRootSource *source);
    static void remove_root_source(GcRootSource *source);

    // 跑一轮标记清扫。
    // 硬前提：当前 C++ 调用栈上不能有任何活的 Ref。
    static void collect();

    // 当前存活对象总数
    [[nodiscard]] static std::size_t live_count();
    // 距上次 collect() 又建了的对象数
    [[nodiscard]] static std::size_t allocated_since_collect();
    // 是否应该 GC，为真就 collect()
    [[nodiscard]] static bool should_collect();
};
