#pragma once

#include <cstddef>

class Object;
class RefVisitor;
template <typename T> class Ref;

/**
 * 堆，串起全部存活对象，并在需要时做一轮 STW 标记清扫。
 * 同时也是唯一引用根。
 */
class Heap {
    friend class Object;
    template <typename T, typename... Args> friend Ref<T> make_ref(Args &&...args);

    // 标记阶段和拆环阶段的两个访问者
    class Marker;
    class RefDropper;

    // 进出全堆链表。在 Object 的构造/析构里调，那时问不到派生类的大小
    static void link(Object *obj);
    static void unlink(Object *obj);

    // 记一笔又分配了多少字节
    static void note_allocated(const Object *obj);

  public:
    Heap() = delete;

    // 跑一轮标记清扫。
    // 硬前提：当前 C++ 调用栈上不能有任何活的 Ref。
    static void collect();

    // 当前存活对象总数
    [[nodiscard]] static std::size_t live_count();
    // 上一轮 collect() 结束时的存活字节数。中途只增不减，是个下界
    [[nodiscard]] static std::size_t live_bytes();
    // 距上次 collect() 又分配了多少字节。分配压力，不是存活量
    [[nodiscard]] static std::size_t bytes_since_collect();
    // 是否该 GC，为真就 collect()
    [[nodiscard]] static bool should_collect();
};
