#pragma once

#include <concepts>
#include <cstddef>
#include <utility>

class Object;
class Type;

// 裸 Object* 一律是借用，不带所有权；持有所有权只能用 Ref
void incref(Object *obj);
void decref(Object *obj);

// 侵入式引用计数句柄。构造恒 +1、析构恒 -1
template <typename T> class Ref {
    T *ptr_{nullptr};

    template <typename U> friend class Ref;

  public:
    Ref() = default;
    explicit(false) Ref(std::nullptr_t) {}
    explicit Ref(T *ptr) : ptr_{ptr} {
        if (ptr_) incref(ptr_);
    }

    Ref(const Ref &other) : Ref{other.ptr_} {}
    Ref(Ref &&other) noexcept : ptr_{std::exchange(other.ptr_, nullptr)} {}

    template <typename U>
        requires std::convertible_to<U *, T *>
    explicit(false) Ref(const Ref<U> &other) : Ref{static_cast<T *>(other.ptr_)} {}
    template <typename U>
        requires std::convertible_to<U *, T *>
    explicit(false) Ref(Ref<U> &&other) noexcept
        : ptr_{static_cast<T *>(std::exchange(other.ptr_, nullptr))} {}

    ~Ref() {
        if (ptr_) decref(ptr_);
    }

    Ref &operator=(Ref other) noexcept {
        std::swap(ptr_, other.ptr_);
        return *this;
    }

    [[nodiscard]] T *get() const { return ptr_; }
    T *operator->() const { return ptr_; }
    T &operator*() const { return *ptr_; }
    explicit operator bool() const { return ptr_; }

    // 指针相等，正好就是 SL 的 is
    [[nodiscard]] bool operator==(const Ref &other) const { return ptr_ == other.ptr_; }
    [[nodiscard]] bool operator==(const T *other) const { return ptr_ == other; }

    void reset() {
        if (ptr_) decref(ptr_);
        ptr_ = nullptr;
    }
};

using ObjectRef = Ref<Object>;

// 建一个新对象并接管它（refcount 0 -> 1）。所有 SL 对象都该经由它建立
template <typename T, typename... Args> [[nodiscard]] Ref<T> make_ref(Args &&...args) {
    return Ref<T>{new T(std::forward<Args>(args)...)};
}

// 遍历一个对象直接强引用的每个槽位。
//
// **GC 的标记阶段和清理阶段共用这一个入口**：标记只读、清理顺带把槽位置空。这样"标记时扫到的边"
// 和"清理时放掉的边"在结构上就不可能不一致——拆成 trace()/clear() 两个方法各写一遍才可能不一致，
// 而那种不一致的后果是提前回收（标记漏了）或永久泄漏（清理漏了），都极难查
class RefVisitor {
  protected:
    // 报告一条出边，target 可能为空
    virtual void visit_ref(Object *target) = 0;
    // 报告完是否顺带把这个槽位置空。GC 的清理阶段返回 true，标记阶段返回 false
    [[nodiscard]] virtual bool clears() const = 0;

  public:
    RefVisitor() = default;
    RefVisitor(const RefVisitor &) = delete;
    RefVisitor &operator=(const RefVisitor &) = delete;
    virtual ~RefVisitor() = default;

    template <typename T> void operator()(Ref<T> &slot) {
        visit_ref(slot.get());
        if (clears()) slot.reset();
    }

    // 容器里每个槽位都过一遍（vector<Ref<...>> 这类）
    template <typename C> void visit_each(C &slots) {
        for (auto &slot : slots) (*this)(slot);
    }
};

// 一切 SL 对象的基类。
//
// **C++ 的类继承 ≠ SL 的类继承**：C++ 这边只表达"存储形状"（有哪些字段、怎么析构），
// SL 那边的继承关系全存在 Type 的 bases_/mro_ 里，两套互不牵连。所以 Bool 不是 Int 的
// C++ 子类（SL 里 bool 也不是 int 的子类，见 SL.md 4.4），而 numbers.Real 这种抽象类
// 压根没有对应的 C++ 类——它只是一个 Type 对象。
class Object {
    // 引用计数。SL 现在是单线程的（以后会有线程，那时这里要重新审视），所以是普通整数不是 atomic
    std::size_t refcount_{0};
    // 所属类型，强引用——用户定义的类会死，实例必须钉住自己的类。
    // 它由 visit_all_refs 统一报告/清理，子类的 visit_own_refs 不用管它
    Ref<Type> type_;

    // 全堆链表的两个链接。Heap 用它枚举整个堆做清扫，Object 自己只在构造/析构时进出链表
    Object *gc_prev_{nullptr};
    Object *gc_next_{nullptr};
    // 标记位。只在一次 collect() 内部有意义，collect() 结束时保证全部复位成 false
    bool gc_marked_{false};

    friend class Heap;
    friend class Runtime;
    // 只给 bootstrap 用：object/type 互为对方的类型，建立时先留空、之后回填。见 Runtime
    void set_type(Type *type);

  protected:
    explicit Object(Type *type);

    // 报告本对象**自己的**强引用槽位（不含上面的 type_）。
    // 纯虚：新增对象类型时漏写是**编译期**错误。但注意"报漏一个字段"编译器管不了——
    // 加字段时必须同步改这里，这是这一层最难查的 bug 类型
    virtual void visit_own_refs(RefVisitor &visitor) = 0;

  public:
    Object(const Object &) = delete;
    Object(Object &&) = delete;
    Object &operator=(const Object &) = delete;
    Object &operator=(Object &&) = delete;
    virtual ~Object();

    [[nodiscard]] Type *type() const { return type_.get(); }
    [[nodiscard]] std::size_t refcount() const { return refcount_; }

    // 本对象的全部出边 = 所属类型 + 子类自己的槽位。**只给 GC 调**，普通代码不该碰
    void visit_all_refs(RefVisitor &visitor);

    // 标记位的读写。同样**只给 GC 调**；一次 collect() 之外它恒为 false
    [[nodiscard]] bool gc_marked() const { return gc_marked_; }
    void gc_set_marked(const bool marked) { gc_marked_ = marked; }

    friend void incref(Object *obj);
    friend void decref(Object *obj);
};
