#include "Heap.h"

#include "Object.h"

#include <cassert>
#include <vector>

namespace {

Object *g_head{nullptr};                    // 全堆链表的头
std::size_t g_live_count{0};                // 当前存活对象总数
std::size_t g_allocated_since_collect{0};   // 距上次 collect() 又建了的对象数
std::vector<GcRootSource *> g_root_sources; // 所有引用根

// 攒够这么多新对象就值得扫一轮
constexpr std::size_t kMinAllocationsBetweenCollects{1024};

} // namespace

/**
 * 标记阶段，把可达对象标记
 */
class Heap::Marker final : public RefVisitor {
    std::vector<Object *> pending_;

  protected:
    void visit_ref(Object *const target) override {
        if (!target || target->gc_marked_) return;
        target->gc_marked_ = true;
        // 显式工作栈，不递归——对象图的深度是用户数据说了算的，递归会爆 C++ 栈
        pending_.push_back(target);
    }

    [[nodiscard]] bool clears() const override { return false; }

  public:
    void drain() {
        while (!pending_.empty()) {
            Object *const object{pending_.back()};
            pending_.pop_back();
            object->visit_all_refs(*this);
        }
    }
};

/**
 * 清理阶段，把垃圾对象的每条出边就地放掉
 */
class Heap::Clearer final : public RefVisitor {
  protected:
    void visit_ref(Object *) override {}
    [[nodiscard]] bool clears() const override { return true; }
};

void Heap::link(Object *const obj) {
    obj->gc_next_ = g_head;
    if (g_head) g_head->gc_prev_ = obj;
    g_head = obj;

    ++g_live_count;
    ++g_allocated_since_collect;
}

void Heap::unlink(Object *const obj) {
    if (obj->gc_prev_)
        obj->gc_prev_->gc_next_ = obj->gc_next_;
    else
        g_head = obj->gc_next_;
    if (obj->gc_next_) obj->gc_next_->gc_prev_ = obj->gc_prev_;

    obj->gc_prev_ = nullptr;
    obj->gc_next_ = nullptr;
    --g_live_count;
}

void Heap::add_root_source(GcRootSource *const source) { g_root_sources.push_back(source); }

void Heap::remove_root_source(GcRootSource *const source) { std::erase(g_root_sources, source); }

std::size_t Heap::live_count() { return g_live_count; }

std::size_t Heap::allocated_since_collect() { return g_allocated_since_collect; }

bool Heap::should_collect() {
    return g_allocated_since_collect >= kMinAllocationsBetweenCollects &&
           g_allocated_since_collect >= g_live_count;
}

void Heap::collect() {
    // ——— 1. 标记 ———
    Marker marker;
    for (GcRootSource *const source : g_root_sources) {
        source->visit_roots(marker);
        marker.drain();
    }

    // ——— 2. 分离：未标记的就是垃圾；顺手把标记复位，省一遍扫描 ———
    std::vector<Object *> garbage;
    for (Object *object{g_head}; object; object = object->gc_next_) {
        if (object->gc_marked_)
            object->gc_marked_ = false;
        else
            garbage.push_back(object);
    }

    // ——— 3. 保命：每个垃圾对象先 +1 ———
    // 下一步放边时，垃圾之间互相持有的引用会归还，谁的计数先归零谁就地析构，
    // 而它析构时又要 decref 别的垃圾——那些可能已经被删过了。这一轮 +1 把整批的生死
    // 统一推迟到第 5 步，析构顺序就不再是个问题
    for (Object *const object : garbage) object->incref();

    // ——— 4. 清理：放掉垃圾的每条出边 ———
    // 指向存活对象的引用在这里被正确归还（不归还就是永久泄漏）；指向垃圾的引用有第 3 步兜着
    Clearer clearer;
    for (Object *const object : garbage) object->visit_all_refs(clearer);

    // ——— 5. 释放 ———
    // 走到这里每个垃圾对象的计数都该恰好是第 3 步加的那个 1：所有指向它的引用要么来自垃圾
    // （第 4 步放掉了），要么来自存活对象——而那意味着它根可达、不该在这批里
    for (Object *const object : garbage) {
        assert(object->refcount() == 1 && "垃圾对象上还挂着计数不明的引用");
        object->decref();
    }

    g_allocated_since_collect = 0;
}
