#pragma once

#include <cstddef>

class Object;
class RefVisitor;

// 提供 GC 根的东西。实现它并注册进 Heap，回收时就会被问"你手上攥着哪些对象"。
// 现在只有 Runtime（内置类型 + 单例）；以后还有帧栈、模块表、每份 Code 的常量表
class GcRootSource {
  public:
    GcRootSource() = default;
    GcRootSource(const GcRootSource &) = delete;
    GcRootSource &operator=(const GcRootSource &) = delete;
    virtual ~GcRootSource() = default;

    // 报告本源持有的每个根槽位。GC 只会拿"不清空"的访问者来问，根永远不会被就地抹掉
    virtual void visit_roots(RefVisitor &visitor) = 0;
};

// 堆：串起全部存活对象，并在需要时做一轮 STW 标记清扫。
//
// 分工是 SL.md 定的"引用计数为主、堆扫描处理循环引用"：绝大多数垃圾在最后一个 Ref 析构时
// 就地释放，collect() 只负责引用计数够不着的那部分——环。
//
// **调用 collect() 的硬前提：当前 C++ 调用栈上不能有任何活的 Ref。**
// 根集合只包含注册进来的那些源，C++ 栈上的局部 Ref 谁也扫不到，扫描时它们指向的对象会被
// 当成不可达清掉。所以回收只能发生在**主循环两条指令之间的安全点**，不能由分配动作顺手触发，
// 更不能在 codegen / 内置函数实现的半途调用——那些地方 C++ 栈上全是局部 Ref。
// 这条前提能成立，靠的是 bytecode.md 那条"C++ 调用栈深度不得随 SL 帧栈深度增长"的约束：
// 执行状态全在堆上的帧对象里，安全点上 C++ 栈本来就是空的。
class Heap {
  public:
    Heap() = delete;

    static void add_root_source(GcRootSource *source);
    static void remove_root_source(GcRootSource *source);

    // 跑一轮标记清扫。前提见上面
    static void collect();

    // 当前存活对象总数
    [[nodiscard]] static std::size_t live_count();
    // 距上次 collect() 又建了多少对象
    [[nodiscard]] static std::size_t allocated_since_collect();
    // 攒够了没有。主循环在安全点问这个，为真就 collect()。阈值是个先能跑的粗策略，
    // 等主循环真跑起来、能量出实际分配速率了再调
    [[nodiscard]] static bool should_collect();

    // 只给 Object 的构造/析构用：进出全堆链表
    static void link(Object *obj);
    static void unlink(Object *obj);
};
