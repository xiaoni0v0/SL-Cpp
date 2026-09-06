#include "Code.h"

#include "../Runtime.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <iterator>
#include <utility>

namespace {

// 一张 vector<Ref<...>> 自己占的堆字节数。指向的对象各自算各自的，不重复计入
template <typename T> std::size_t table_bytes(const std::vector<T> &table) {
    return table.capacity() * sizeof(T);
}

} // namespace

Code::Code(Parts parts) : Object{Runtime::type_code()}, parts_{std::move(parts)} {
    assert(
        parts_.ref_captured.size() <= parts_.names.size() &&
        "ref_captured 是 names 的位图，不能比 names 长"
    );
}

bool Code::is_ref_captured(const std::size_t index) const {
    assert(index < parts_.names.size());

    // 允许位图短于 names：尾部没标记的一律不是引用捕获
    return index < parts_.ref_captured.size() && parts_.ref_captured[index];
}

std::optional<Code::LineEntry> Code::line_at(const std::uint32_t pc) const {
    // 找最后一个 entry.pc <= pc 的项
    const auto it{std::ranges::upper_bound(parts_.lines, pc, {}, &LineEntry::pc)};
    if (it == parts_.lines.begin()) return std::nullopt;

    return *std::prev(it);
}

void Code::visit_own_refs(RefVisitor &visitor) {
    // 加了新的引用字段一定要在这里补一笔，漏了就是提前回收
    for (RefBase &ref : parts_.constants) visitor.visit(ref);
    for (RefBase &ref : parts_.nested_codes) visitor.visit(ref);
    for (RefBase &ref : parts_.names) visitor.visit(ref);
    for (RefBase &ref : parts_.value_captured) visitor.visit(ref);

    if (parts_.params) {
        for (OneParam &param : parts_.params->positional) visitor.visit(param.name);
        for (OneParam &param : parts_.params->kw_only) visitor.visit(param.name);
        visitor.visit(parts_.params->var_args);
        visitor.visit(parts_.params->var_kwargs);
    }
}

std::size_t Code::size_bytes() const {
    std::size_t bytes{
        sizeof(*this) + table_bytes(parts_.bytecode) + table_bytes(parts_.constants) +
        table_bytes(parts_.nested_codes) + table_bytes(parts_.names) +
        table_bytes(parts_.value_captured) + table_bytes(parts_.lines) +
        parts_.ref_captured.capacity() / CHAR_BIT + parts_.source_name.capacity()
    };

    if (parts_.params) {
        bytes += table_bytes(parts_.params->positional) + table_bytes(parts_.params->kw_only);
    }

    return bytes;
}
