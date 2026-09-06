#include "Code.h"

#include "../../utils/memory_utils.h"
#include "../Runtime.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <utility>

Code::Code(Parts parts) : Object{Runtime::type_code()}, parts_{std::move(parts)} {
    assert(
        parts_.ref_captured.size() <= parts_.names_table.size() &&
        "ref_captured 是 names 的位图，不能比 names 长"
    );
}

bool Code::is_ref_captured(const std::size_t index) const {
    assert(index < parts_.names_table.size());

    // 尾部没标记的一律不是引用捕获
    return index < parts_.ref_captured.size() && parts_.ref_captured[index];
}

std::optional<Code::LineEntry> Code::line_at(const std::uint32_t pc) const {
    // 找最后一个 entry.pc <= pc 的项
    const auto it{std::ranges::upper_bound(parts_.lines_table, pc, {}, &LineEntry::pc)};
    if (it == parts_.lines_table.begin()) return std::nullopt;

    return *std::prev(it);
}

void Code::visit_own_refs(RefVisitor &visitor) {
    for (RefBase &ref : parts_.constants_table) visitor.visit(ref);
    for (RefBase &ref : parts_.codes_table) visitor.visit(ref);
    for (RefBase &ref : parts_.names_table) visitor.visit(ref);
    for (RefBase &ref : parts_.value_captured) visitor.visit(ref);

    if (parts_.params) {
        for (OneParamShape &param : parts_.params->positional) visitor.visit(param.name);
        for (OneParamShape &param : parts_.params->kw_only) visitor.visit(param.name);
        visitor.visit(parts_.params->var_args);
        visitor.visit(parts_.params->var_kwargs);
    }
}

std::size_t Code::size_bytes() const {
    return sizeof(*this) + mem::heap_bytes(parts_.bytecode) +
           mem::heap_bytes(parts_.constants_table) + mem::heap_bytes(parts_.codes_table) +
           mem::heap_bytes(parts_.names_table) + mem::heap_bytes(parts_.ref_captured) +
           mem::heap_bytes(parts_.value_captured) + mem::heap_bytes(parts_.params) +
           mem::heap_bytes(parts_.lines_table) + mem::heap_bytes(parts_.source_name);
}
