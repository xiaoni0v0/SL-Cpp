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

/**
 * 一切 SL 对象的基类。
 *
 * C++ 的类继承 ≠ SL 的类继承，SL 继承关系全存在 Type 的 bases_/mro_ 里。
 */
class Object {
    // 引用计数
    std::size_t refcount_{0};
    // 类型
    Ref<Type> type_;

    // 全堆链表，侵入式
    Object *gc_prev_{nullptr};
    Object *gc_next_{nullptr};
    // 标记位，只在一次 collect() 内部有意义
    bool gc_marked_{false};

    friend class Heap;
    friend class Runtime;

    // 只给 bootstrap 用的后门，处理 object/type 关系
    void set_type(Type *type);

  protected:
    explicit Object(Type *type);

    // 报告本对象自己（不含上面的 type_）的强引用
    virtual void visit_own_refs(RefVisitor &visitor) = 0;

  public:
    Object(const Object &) = delete;
    Object(Object &&) = delete;
    Object &operator=(const Object &) = delete;
    Object &operator=(Object &&) = delete;
    virtual ~Object();

    [[nodiscard]] Type *type() const { return type_.get(); }
    [[nodiscard]] std::size_t refcount() const { return refcount_; }

    // 只给 GC 用的后门，本对象的全部出边 = 所属类型的强引用 + 子类自己的的强引用
    void visit_all_refs(RefVisitor &visitor);

    // 只给 GC 用的后门，标记位的读写
    [[nodiscard]] bool gc_marked() const { return gc_marked_; }
    void gc_set_marked(const bool marked) { gc_marked_ = marked; }

    friend void incref(Object *obj);
    friend void decref(Object *obj);
};
