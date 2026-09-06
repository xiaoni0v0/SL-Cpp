#pragma once

#include "Heap.h"

#include <concepts>
#include <cstddef>
#include <utility>

class Object;
class Type;

/**
 * 一条引用关系里跟 T 无关的那一半，指向某个 Object。
 *
 * 只能作为 Ref<T> 的基类。
 */
class RefBase {
    friend class Heap;

  protected:
    Object *ptr_{nullptr};

    RefBase() = default;
    // +1
    explicit RefBase(Object *ptr);
    // -1
    ~RefBase();

    // 放掉这条引用并置空
    void reset();

  public:
    RefBase(const RefBase &) = delete;
    RefBase &operator=(const RefBase &) = delete;

    // 指向的对象，可能为空。借用，不改引用计数
    [[nodiscard]] Object *target() const { return ptr_; }

    [[nodiscard]] static constexpr std::size_t heap_bytes() { return 0; }
};

/**
 * 一条引用关系。
 *
 * 会改对象的引用计数，构造 Ref 恒 +1、析构 Ref 恒 -1。
 */
template <typename T> class Ref final : public RefBase {
    template <typename U> friend class Ref;

  public:
    Ref() = default;
    explicit(false) Ref(std::nullptr_t) {}
    explicit Ref(T *ptr) : RefBase{ptr} {}

    Ref(const Ref &other) : RefBase{other.ptr_} {}
    Ref(Ref &&other) noexcept { ptr_ = std::exchange(other.ptr_, nullptr); }

    template <typename U>
        requires std::convertible_to<U *, T *>
    explicit(false) Ref(const Ref<U> &other) : RefBase{other.ptr_} {}
    template <typename U>
        requires std::convertible_to<U *, T *>
    explicit(false) Ref(Ref<U> &&other) noexcept {
        ptr_ = std::exchange(other.ptr_, nullptr);
    }

    Ref &operator=(Ref other) noexcept {
        std::swap(ptr_, other.ptr_);
        return *this;
    }

    // 放掉自己这条引用并置空，等价于 `*this = nullptr`
    void reset() { RefBase::reset(); }

    [[nodiscard]] T *get() const { return static_cast<T *>(ptr_); }
    T *operator->() const { return get(); }
    T &operator*() const { return *get(); }
    explicit operator bool() const { return ptr_; }

    // 指针相等，对应 SL 的 is
    [[nodiscard]] bool operator==(const Ref &other) const { return ptr_ == other.ptr_; }
    [[nodiscard]] bool operator==(const T *other) const { return ptr_ == other; }
};

using ObjectRef = Ref<Object>;

// 所有 SL 对象都该经由它建立
template <typename T, typename... Args> [[nodiscard]] Ref<T> make_ref(Args &&...args) {
    T *const object{new T(std::forward<Args>(args)...)};
    Heap::note_allocated(object);
    return Ref<T>{object};
}

// 只能经由 make_ref 建立，构造函数放进 private，再写一行 `SL_MAKE_REF_ONLY;`
#define SL_MAKE_REF_ONLY                                                                           \
    template <typename T, typename... Args> friend Ref<T> make_ref(Args &&...args)

/**
 * 遍历一个对象每个直接强引用。
 *
 * 拿到的是引用本身而不是它指向的对象，所以"看一眼"和"放掉"两种用法共用同一份字段清单
 * （见 Object::visit_own_refs），标记扫到的边和拆环放掉的边不可能对不上。
 */
class RefVisitor {
  public:
    RefVisitor() = default;
    RefVisitor(const RefVisitor &) = delete;
    RefVisitor &operator=(const RefVisitor &) = delete;
    virtual ~RefVisitor() = default;

    // 对一条引用调用一次
    virtual void visit(RefBase &ref) = 0;
};

/**
 * 一切 SL 对象的基类。
 *
 * C++ 的类继承 ≠ SL 的类继承，SL 继承关系全存在 Type 的 bases_/mro_ 里。
 */
class Object {
    friend class Heap;
    friend class RefBase;

    // 引用计数
    std::size_t refcount_{0};
    // 类型
    Ref<Type> type_;

    // 全堆链表，侵入式
    Object *gc_prev_{nullptr};
    Object *gc_next_{nullptr};
    // 是否可达；只在一次 collect() 内部有意义，collect() 结束时保证全部复位回 false
    bool gc_reachable_{false};

    // 引用计数的加减
    void incref();
    void decref();

    // 遍历本对象的全部强引用，= 所属类型的强引用 + 子类自己的强引用
    void visit_refs(RefVisitor &visitor);
    // 本对象自己（不含上面的 type_）的强引用。
    virtual void visit_own_refs(RefVisitor &visitor) = 0;

  protected:
    explicit Object(Type *type);

    // 改写自己的类型。唯一的用途是 bootstrap 回填 object/type 的类
    void set_type(Type *type);

  public:
    Object(const Object &) = delete;
    Object(Object &&) = delete;
    Object &operator=(const Object &) = delete;
    Object &operator=(Object &&) = delete;
    virtual ~Object();

    [[nodiscard]] Type *type() const;

    [[nodiscard]] std::size_t refcount() const { return refcount_; }

    // 本对象占用的堆字节数，= sizeof(自己) + 自己独占的其他堆分配
    [[nodiscard]] virtual std::size_t size_bytes() const = 0;
};
