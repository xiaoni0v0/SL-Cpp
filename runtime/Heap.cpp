#include "Heap.h"

#include "../cppexceptions/InternalError.h"
#include "Object.h"

#include <vector>

namespace {

Object *g_head{nullptr};                    // 全堆链表的头
std::size_t g_live_count{0};                // 当前存活对象总数
std::size_t g_live_bytes{0};                // 上一轮 collect() 结束时的存活字节数
std::size_t g_bytes_since_collect{0};       // 距上次 collect() 又分配了多少字节
std::vector<GcRootSource *> g_root_sources; // 所有引用根

// 分配压力攒够这么多字节就值得扫一轮
constexpr std::size_t kMinBytesBetweenCollects{1 << 20};

} // namespace

/**
 * 标记阶段，把可达对象标记
 */
class Heap::Marker final : public RefVisitor {
    std::vector<Object *> pending_; // 显式工作栈，不递归

  public:
    void visit(RefBase &ref) override {
        Object *const target{ref.target()};
        if (!target || target->gc_reachable_) return;
        target->gc_reachable_ = true;
        pending_.push_back(target);
    }

    void drain() {
        while (!pending_.empty()) {
            Object *const object{pending_.back()};
            pending_.pop_back();
            object->visit_refs(*this);
        }
    }
};

/**
 * 拆环阶段，把垃圾对象的每条出边就地放掉
 */
class Heap::RefDropper final : public RefVisitor {
  public:
    void visit(RefBase &ref) override { ref.reset(); }
};

void Heap::link(Object *const obj) {
    // 顶掉原来的 head
    obj->gc_next_ = g_head;
    if (g_head) g_head->gc_prev_ = obj;
    g_head = obj;

    ++g_live_count;
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

void Heap::note_allocated(const Object *const obj) { g_bytes_since_collect += obj->size_bytes(); }

void Heap::add_root_source(GcRootSource *const source) { g_root_sources.push_back(source); }

void Heap::remove_root_source(GcRootSource *const source) { std::erase(g_root_sources, source); }

void Heap::collect() {
    // 1. 标记
    Marker marker;
    for (GcRootSource *const source : g_root_sources) {
        source->visit_roots(marker);
        marker.drain();
    }

    // 2. 分离
    std::vector<Object *> garbage;
    g_live_bytes = 0;
    for (Object *object{g_head}; object; object = object->gc_next_) {
        if (object->gc_reachable_) {
            // 顺手把标记复位、算字节
            object->gc_reachable_ = false;
            g_live_bytes += object->size_bytes();
        } else {
            // 未标记的就是垃圾
            garbage.push_back(object);
        }
    }

    // 3. 保命，每个垃圾对象先 +1
    for (Object *const object : garbage) object->incref();

    // 4. 拆环：释放垃圾对象的每条出边
    RefDropper dropper;
    for (Object *const object : garbage) object->visit_refs(dropper);

    // 5. 销毁
    for (Object *const object : garbage) {
        if (object->refcount() != 1)
            throw InternalError{
                "GC: garbage object still referenced (collect() called outside a safe point?)"
            };

        object->decref();
    }

    g_bytes_since_collect = 0;
}

std::size_t Heap::live_count() { return g_live_count; }

std::size_t Heap::live_bytes() { return g_live_bytes; }

std::size_t Heap::bytes_since_collect() { return g_bytes_since_collect; }

bool Heap::should_collect() {
    return g_bytes_since_collect >= kMinBytesBetweenCollects &&
           g_bytes_since_collect >= g_live_bytes;
}
