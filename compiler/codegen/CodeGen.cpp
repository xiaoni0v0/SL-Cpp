#include "CodeGen.h"

#include "../../cppexceptions/InternalError.h"
#include "../../numeric/BigDec.h"
#include "../../numeric/BigInt.h"
#include "../../runtime/Runtime.h"
#include "../../runtime/objects/Bool.h"
#include "../../runtime/objects/Decimal.h"
#include "../../runtime/objects/Int.h"
#include "../../runtime/objects/NamedSingleton.h"
#include "../../runtime/objects/Str.h"
#include "../../runtime/objects/Tuple.h"
#include "../../utils/string_utils.h"

#include <cassert>
#include <utility>

namespace {

// AST 的二元运算符 -> 字节码的。and / or 不在这里，它们走短路，不落在 BINARY_OP 上
BinaryOp to_binary_op(const AstNodeOpBinary::OpType op) {
    using AstOp = AstNodeOpBinary::OpType;

    switch (op) {
    case AstOp::Add:
        return BinaryOp::Add;
    case AstOp::Sub:
        return BinaryOp::Sub;
    case AstOp::Mul:
        return BinaryOp::Mul;
    case AstOp::Div:
        return BinaryOp::Div;
    case AstOp::DivFloor:
        return BinaryOp::DivFloor;
    case AstOp::Mod:
        return BinaryOp::Mod;
    case AstOp::Pow:
        return BinaryOp::Pow;
    case AstOp::BitAnd:
        return BinaryOp::BitAnd;
    case AstOp::BitOr:
        return BinaryOp::BitOr;
    case AstOp::BitXor:
        return BinaryOp::BitXor;
    case AstOp::LShift:
        return BinaryOp::LShift;
    case AstOp::RShift:
        return BinaryOp::RShift;
    case AstOp::Range:
        return BinaryOp::Range;
    case AstOp::In:
        return BinaryOp::In;
    case AstOp::And:
    case AstOp::Or:
        break;
    }

    return BinaryOp::Add; // 调用方保证不会走到，外面另有 InternalError 兜着
}

BinaryOp to_binary_op(const AstNodeCompare::OpType op) {
    using AstOp = AstNodeCompare::OpType;

    switch (op) {
    case AstOp::Lt:
        return BinaryOp::Lt;
    case AstOp::Le:
        return BinaryOp::Le;
    case AstOp::Gt:
        return BinaryOp::Gt;
    case AstOp::Ge:
        return BinaryOp::Ge;
    case AstOp::Eq:
        return BinaryOp::Eq;
    case AstOp::Ne:
        return BinaryOp::Ne;
    }

    std::unreachable();
}

UnaryOp to_unary_op(const AstNodeOpUnary::OpType op) {
    using AstOp = AstNodeOpUnary::OpType;

    switch (op) {
    case AstOp::Question:
        return UnaryOp::Question;
    case AstOp::Exclaim:
        return UnaryOp::Exclaim;
    case AstOp::Pos:
        return UnaryOp::Pos;
    case AstOp::Neg:
        return UnaryOp::Neg;
    case AstOp::BitInvert:
        return UnaryOp::BitInvert;
    case AstOp::Not:
        return UnaryOp::Not;
    }

    std::unreachable();
}

} // namespace

CodeGen::CodeGen(std::string file_path) : file_path_{std::move(file_path)} {}

CodeBuilder &CodeGen::code() const {
    assert(!builders_.empty() && "没有正在填的 Code");

    return *builders_.back();
}

void CodeGen::error_internal(const std::string &msg, const Position pos) const {
    throw InternalError{file_path_, pos.row, pos.col, msg};
}

// ============================================================
// 编译一个节点
// ============================================================

void CodeGen::gen(const AstNode &node, const bool want_value) {
    [[maybe_unused]] const std::uint32_t before{code().stack_depth()};
    [[maybe_unused]] const bool was_reachable{!code().unreachable()};

    want_value_ = want_value;
    node.accept(*this);

    assert(
        (!was_reachable || code().unreachable() ||
         code().stack_depth() == before + (want_value ? 1u : 0u)) &&
        "编译一个节点，栈深必须恰好按 want_value 变化"
    );
}

void CodeGen::gen(const AstNodePtr &node, const bool want_value) {
    if (!node) error_internal("unexpected null node", Position{0, 0});

    gen(*node, want_value);
}

void CodeGen::gen_program(const AstNodeProgram &program) {
    // Program 的值只由 return 决定，跟复合表达式不同：这里每一条都不产出值
    for (const AstNodePtr &expr : program.exprs_) gen(expr, false);

    // 一路没触发 return 就走到这里。Program 的值是 None，等价于末尾隐含一条 return None
    if (code().unreachable()) return;

    gen_none(program.pos_);
    code().emit(OpCode::RETURN_VALUE, program.pos_);
}

void CodeGen::gen_exprs(
    const std::vector<AstNodePtr> &exprs, const bool want_value, const Position pos
) {
    if (exprs.empty()) {
        if (want_value) gen_none(pos);
        return;
    }

    // 非末尾的那些不产出值
    for (std::size_t i{0}; i + 1 < exprs.size(); ++i) gen(exprs[i], false);
    gen(exprs.back(), want_value);
}

// ============================================================
// 小零件
// ============================================================

void CodeGen::gen_load_const(const ObjectRef &value, const Position pos) {
    code().emit(OpCode::LOAD_CONST, code().intern_const(value), pos);
}

void CodeGen::gen_none(const Position pos) {
    gen_load_const(ObjectRef{Runtime::singleton_none()}, pos);
}

void CodeGen::gen_missing(const Position pos) {
    code().emit(OpCode::LOAD_INTERNAL, static_cast<std::uint32_t>(InternalObject::Missing), pos);
}

void CodeGen::gen_optional(const AstNodePtr &node, const Position pos) {
    if (node)
        gen(*node, true);
    else
        gen_missing(pos);
}

void CodeGen::gen_pop_to(const std::uint32_t depth, const Position pos) {
    if (code().unreachable()) return;

    assert(code().stack_depth() >= depth && "要裁到的深度比当前还深");
    while (code().stack_depth() > depth) code().emit(OpCode::POP_TOP, pos);
}

void CodeGen::gen_jump_if_false(const AstNode &cond, const CodeBuilder::Label target) {
    gen(cond, true);
    code().emit(OpCode::TO_BOOL, cond.pos_);
    code().emit_jump(OpCode::JUMP_IF_FALSE, target, cond.pos_);
}

void CodeGen::gen_unwind(
    const std::size_t to_block_depth, const std::uint32_t target_depth, const Position pos
) {
    // 由内到外逐层退出。块栈是运行期的，跳走之前必须一层层弹干净，否则跳到循环外之后
    // 再抛异常，还会撞进这些已经失效的处理器
    for (std::size_t i{blocks_.size()}; i > to_block_depth && !code().unreachable(); --i) {
        const BlockContext &block{blocks_[i - 1]};

        code().emit(OpCode::POP_BLOCK, pos);
        if (!block.is_finally_) continue;

        // 进 finally 体时栈顶恒是"这条路径要带出去的那个值"。break/continue 没有值可带，
        // 压个 None 占位，回来再弹掉
        gen_pop_to(block.entry_depth_, pos);
        gen_none(pos);
        code().emit_jump(OpCode::CALL_FINALLY, block.finally_label_, pos);
        code().emit(OpCode::POP_TOP, pos);
    }

    gen_pop_to(target_depth, pos);
}

void CodeGen::gen_unwind_with_value(const Position pos) {
    // 返回值就是这条路径要带出去的那个值，本来就在栈顶
    for (std::size_t i{blocks_.size()}; i > 0 && !code().unreachable(); --i) {
        const BlockContext &block{blocks_[i - 1]};

        // 自己这层块得先弹掉，否则 finally 体里抛的异常会被它自己接住
        code().emit(OpCode::POP_BLOCK, pos);
        if (!block.is_finally_) continue;

        // 把返回值沉下去，弹掉夹在中间的临时值
        if (const std::uint32_t extra{code().stack_depth() - block.entry_depth_ - 1}; extra > 0) {
            code().emit(OpCode::INSERT, extra + 1, pos);
            for (std::uint32_t k{0}; k < extra; ++k) code().emit(OpCode::POP_TOP, pos);
        }
        code().emit_jump(OpCode::CALL_FINALLY, block.finally_label_, pos);
    }
}

// ============================================================
// 常量
// ============================================================

ObjectRef CodeGen::literal_object(const AstNode &node) {
    if (dynamic_cast<const AstNodeLiteralNone *>(&node))
        return ObjectRef{Runtime::singleton_none()};
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&node))
        return ObjectRef{Runtime::singleton_ellipsis()};
    if (const auto *const flag{dynamic_cast<const AstNodeLiteralBool *>(&node)})
        return ObjectRef{Runtime::singleton_bool(flag->value_)};
    if (const auto *const text{dynamic_cast<const AstNodeLiteralStr *>(&node)})
        return make_ref<Str>(text->value_);
    if (const auto *const number{dynamic_cast<const AstNodeLiteralInt *>(&node)})
        return make_ref<Int>(BigInt::from_decimal_string(u32_to_utf8(number->raw_)));

    if (const auto *const number{dynamic_cast<const AstNodeLiteralDecimal *>(&node)}) {
        // 构造不舍入、精确保留全部位数，所以不查上下文
        const std::optional value{BigDec::try_from_string(u32_to_utf8(number->raw_))};
        if (!value) return {};

        return make_ref<Decimal>(*value);
    }

    // 元组自底向上：元素全是常量才算，有一个不是整个就得现建
    if (const auto *const tuple{dynamic_cast<const AstNodeLiteralTuple *>(&node)}) {
        std::vector<ObjectRef> items;
        items.reserve(tuple->items_.size());

        for (const AstNodePtr &item : tuple->items_) {
            if (!item) return {};

            ObjectRef value{literal_object(*item)};
            if (!value) return {};
            items.push_back(std::move(value));
        }

        return make_ref<Tuple>(std::move(items));
    }

    // list / dict 可变，每次求值必须是新对象，不进常量表
    return {};
}

// ============================================================
// 容器
// ============================================================

bool CodeGen::has_expansion(const std::vector<AstNodePtr> &items) {
    for (const AstNodePtr &item : items)
        if (dynamic_cast<const AstNodeStar *>(item.get())) return true;

    return false;
}

void CodeGen::gen_sequence(
    const std::vector<AstNodePtr> &items, const bool as_tuple, const Position pos
) {
    const auto count{static_cast<std::uint32_t>(items.size())};

    // 个数是编译期常数，一条指令建好，不必逐项并入
    if (!has_expansion(items)) {
        for (const AstNodePtr &item : items) gen(item, true);
        code().emit(as_tuple ? OpCode::MAKE_TUPLE : OpCode::MAKE_LIST, count, pos);

        return;
    }

    // 第一个展开项之前的照常逐个求值，一次建好；其后逐项并入。求值顺序与源码一致
    std::size_t head{0};
    while (head < items.size() && !dynamic_cast<const AstNodeStar *>(items[head].get())) ++head;

    for (std::size_t i{0}; i < head; ++i) gen(items[i], true);
    code().emit(OpCode::MAKE_LIST, static_cast<std::uint32_t>(head), pos);

    for (std::size_t i{head}; i < items.size(); ++i) {
        const auto *const star{dynamic_cast<const AstNodeStar *>(items[i].get())};
        gen(star ? star->operand_ : items[i], true);
        code().emit(star ? OpCode::LIST_EXTEND : OpCode::LIST_APPEND, 1, items[i]->pos_);
    }

    if (as_tuple) code().emit(OpCode::LIST_TO_TUPLE, pos);
}

void CodeGen::gen_dict(
    const std::vector<std::pair<AstNodePtr, AstNodePtr>> &items, const Position pos
) {
    // **expr 那一项的 key 是 AstNodeDoubleStar、value 为空
    const auto is_spread{[](const std::pair<AstNodePtr, AstNodePtr> &item) {
        return dynamic_cast<const AstNodeDoubleStar *>(item.first.get()) != nullptr;
    }};

    std::size_t head{0};
    while (head < items.size() && !is_spread(items[head])) ++head;

    if (head == items.size()) {
        for (const auto &[key, value] : items) {
            gen(key, true);
            gen(value, true);
        }
        code().emit(OpCode::MAKE_DICT, static_cast<std::uint32_t>(items.size()), pos);

        return;
    }

    for (std::size_t i{0}; i < head; ++i) {
        gen(items[i].first, true);
        gen(items[i].second, true);
    }
    code().emit(OpCode::MAKE_DICT, static_cast<std::uint32_t>(head), pos);

    for (std::size_t i{head}; i < items.size(); ++i) {
        const auto &[key, value]{items[i]};

        if (const auto *const spread{dynamic_cast<const AstNodeDoubleStar *>(key.get())}) {
            gen(spread->operand_, true);
            code().emit(OpCode::DICT_MERGE, 1, key->pos_);
        } else {
            gen(key, true);
            gen(value, true);
            code().emit(OpCode::DICT_PUT, 1, key->pos_);
        }
    }
}

// ============================================================
// 赋值
// ============================================================

const std::vector<AstNodePtr> *CodeGen::destructuring_items(const AstNode &target) {
    if (const auto *const tuple{dynamic_cast<const AstNodeLiteralTuple *>(&target)})
        return &tuple->items_;
    if (const auto *const list{dynamic_cast<const AstNodeLiteralList *>(&target)})
        return &list->items_;

    return nullptr;
}

std::uint32_t CodeGen::gen_target_prefix(const AstNode &target, const bool under_value) {
    // under_value 时待写入的值已经在栈顶，每压一项就把它沉到值下面
    const auto push{[&](const AstNodePtr &node) {
        gen(node, true);
        if (under_value) code().emit(OpCode::INSERT, 2, node->pos_);
    }};

    if (const auto *const attr{dynamic_cast<const AstNodeAttr *>(&target)}) {
        push(attr->object_);

        return 1;
    }

    if (const auto *const index{dynamic_cast<const AstNodeIndex *>(&target)}) {
        push(index->object_);

        // 下标里出现 *expr 时个数不再是编译期常数，先拼成一个 list
        if (has_expansion(index->args_)) {
            gen_sequence(index->args_, false, index->pos_bracket_);
            if (under_value) code().emit(OpCode::INSERT, 2, index->pos_bracket_);

            return 2;
        }

        for (const AstNodePtr &arg : index->args_) push(arg);

        return 1 + static_cast<std::uint32_t>(index->args_.size());
    }

    // 标识符、解构目标都不需要前置操作数
    return 0;
}

void CodeGen::gen_target_store(const AstNode &target) {
    const Position pos{target.pos_};

    if (const auto *const name{dynamic_cast<const AstNodeIdentifier *>(&target)}) {
        code().emit(OpCode::STORE_NAME, code().intern_name(name->identifier_), pos);

        return;
    }

    if (const auto *const attr{dynamic_cast<const AstNodeAttr *>(&target)}) {
        code().emit(OpCode::SET_ATTR, code().intern_name(attr->attr_), attr->pos_dot_);

        return;
    }

    if (const auto *const index{dynamic_cast<const AstNodeIndex *>(&target)}) {
        if (has_expansion(index->args_))
            code().emit(OpCode::SET_INDEX_EX, index->pos_bracket_);
        else
            code().emit(
                OpCode::SET_INDEX,
                static_cast<std::uint32_t>(index->args_.size()),
                index->pos_bracket_
            );

        return;
    }

    const std::vector<AstNodePtr> *const items{destructuring_items(target)};
    if (!items) error_internal("cannot assign to this expression", pos);

    // 星号左右两侧的项数各占参数里的一个字节，各自不超过 255（SemanticChecker 已经挡过）
    std::size_t star{items->size()};
    for (std::size_t i{0}; i < items->size(); ++i)
        if (dynamic_cast<const AstNodeStar *>((*items)[i].get())) star = i;

    if (star == items->size()) {
        code().emit(OpCode::UNPACK, static_cast<std::uint32_t>(items->size()), pos);
    } else {
        const auto after{static_cast<std::uint32_t>(items->size() - star - 1)};
        code().emit(OpCode::UNPACK_EX, (after << 8u) | static_cast<std::uint32_t>(star), pos);
    }

    // UNPACK 逆序压入，于是第一个元素在栈顶
    for (const AstNodePtr &item : *items) {
        const auto *const item_star{dynamic_cast<const AstNodeStar *>(item.get())};
        gen_store_one(item_star ? *item_star->operand_ : *item);
    }
}

void CodeGen::gen_store_one(const AstNode &target) {
    gen_target_prefix(target, true);
    gen_target_store(target);
}

void CodeGen::gen_target_load(const AstNode &target, const std::uint32_t prefix) {
    const Position pos{target.pos_};

    if (const auto *const name{dynamic_cast<const AstNodeIdentifier *>(&target)}) {
        // 复合赋值的读写必须落在同一帧，所以不走逐级退回的那条
        code().emit(OpCode::LOAD_NAME_STRICT, code().intern_name(name->identifier_), pos);

        return;
    }

    // 前置操作数只求值一次，读旧值靠把它们整体复制一份
    for (std::uint32_t i{0}; i < prefix; ++i) code().emit(OpCode::COPY, prefix, pos);

    if (const auto *const attr{dynamic_cast<const AstNodeAttr *>(&target)}) {
        code().emit(OpCode::GET_ATTR, code().intern_name(attr->attr_), attr->pos_dot_);

        return;
    }

    const auto *const index{dynamic_cast<const AstNodeIndex *>(&target)};
    if (!index) error_internal("cannot compound-assign to this expression", pos);

    if (has_expansion(index->args_))
        code().emit(OpCode::GET_INDEX_EX, index->pos_bracket_);
    else
        code().emit(
            OpCode::GET_INDEX, static_cast<std::uint32_t>(index->args_.size()), index->pos_bracket_
        );
}

// ============================================================
// 调用
// ============================================================

void CodeGen::gen_call_args_packed(const CallArgs &args, const Position pos) {
    gen_sequence(args.positional_args_, false, pos);

    if (args.keyword_args_.empty()) {
        code().emit(OpCode::MAKE_DICT, 0, pos);

        return;
    }

    // 关键字组：identifier = expr 走 DICT_PUT，**expr 走 DICT_MERGE
    code().emit(OpCode::MAKE_DICT, 0, pos);
    for (const OneKwArg &arg : args.keyword_args_) {
        if (arg.kind_ == OneKwArg::Kind::DoubleStar) {
            // 这一档的 value_ 存的是整个 AstNodeDoubleStar，要展开的是它的操作数
            const auto *const spread{dynamic_cast<const AstNodeDoubleStar *>(arg.value_.get())};
            if (!spread) error_internal("malformed ** argument", pos);

            gen(spread->operand_, true);
            code().emit(OpCode::DICT_MERGE, 1, pos);
        } else {
            gen_load_const(make_ref<Str>(arg.keyword_), pos);
            gen(arg.value_, true);
            code().emit(OpCode::DICT_PUT, 1, pos);
        }
    }
}

void CodeGen::gen_call(const CallArgs &args, const Position pos) {
    const bool has_double_star{[&] {
        for (const OneKwArg &arg : args.keyword_args_)
            if (arg.kind_ == OneKwArg::Kind::DoubleStar) return true;

        return false;
    }()};

    // 三条调用指令是按出现频率分的档：CALL 零额外对象，CALL_KW 多一个常量表里的名字元组，
    // CALL_EX 才建两个新容器
    if (has_expansion(args.positional_args_) || has_double_star) {
        gen_call_args_packed(args, pos);
        code().emit(OpCode::CALL_EX, pos);

        return;
    }

    for (const AstNodePtr &arg : args.positional_args_) gen(arg, true);

    if (args.keyword_args_.empty()) {
        code().emit(OpCode::CALL, static_cast<std::uint32_t>(args.positional_args_.size()), pos);

        return;
    }

    std::vector<ObjectRef> names;
    names.reserve(args.keyword_args_.size());
    for (const OneKwArg &arg : args.keyword_args_) {
        gen(arg.value_, true);
        names.push_back(make_ref<Str>(arg.keyword_));
    }

    gen_load_const(make_ref<Tuple>(std::move(names)), pos);
    code().emit(
        OpCode::CALL_KW,
        static_cast<std::uint32_t>(args.positional_args_.size() + args.keyword_args_.size()),
        pos
    );
}

// ============================================================
// 循环
// ============================================================

bool CodeGen::gen_loop_slot(const CollectMark collect, const bool want_value, const Position pos) {
    switch (collect.container_) {
    case CollectMark::Container::None:
        // 计数模式的结果槽是纯 codegen 垫的，没人要值就退化成裸循环
        if (!want_value) return false;
        gen_load_const(make_ref<Int>(BigInt{0}), pos);

        return true;
    case CollectMark::Container::List:
        // 收尾指令自己可能抛 TypeError/ValueError，跳过就是悄悄吞异常，所以省不掉
        code().emit(OpCode::MAKE_LIST, 0, pos);

        return true;
    case CollectMark::Container::Dict:
        code().emit(OpCode::MAKE_DICT, 0, pos);

        return true;
    }

    std::unreachable();
}

void CodeGen::gen_loop_collect(
    const CollectMark collect, const bool has_slot, const std::uint32_t slot_depth,
    const CodeBuilder::Label count_continue, const Position pos
) {
    switch (collect.container_) {
    case CollectMark::Container::None:
        // 本轮的值用不上，弹掉。continue 跳到 POP_TOP 之后：它自己的裁栈已经清掉那个值了
        code().emit(OpCode::POP_TOP, pos);
        code().place_label(count_continue);
        if (has_slot) code().emit(OpCode::COUNT_INC, slot_depth, pos);

        return;
    case CollectMark::Container::List:
        code().emit(collect.expand_ ? OpCode::LIST_EXTEND : OpCode::LIST_APPEND, slot_depth, pos);

        return;
    case CollectMark::Container::Dict:
        if (collect.expand_) {
            code().emit(OpCode::DICT_MERGE, slot_depth, pos);
        } else {
            // UNPACK 2 给出 v k，DICT_PUT 要 d k v
            code().emit(OpCode::UNPACK, 2, pos);
            code().emit(OpCode::INSERT, 2, pos);
            code().emit(OpCode::DICT_PUT, slot_depth, pos);
        }

        return;
    }
}

// ============================================================
// 嵌套作用域
// ============================================================

void CodeGen::push_scope() {
    builders_.push_back(std::make_unique<CodeBuilder>(file_path_));

    // 新作用域是新的一份 Code，外层的循环、finally 在这里都跳不到
    saved_loops_.push_back(std::exchange(loops_, {}));
    saved_blocks_.push_back(std::exchange(blocks_, {}));
}

Ref<Code> CodeGen::pop_scope() {
    loops_ = std::move(saved_loops_.back());
    saved_loops_.pop_back();
    blocks_ = std::move(saved_blocks_.back());
    saved_blocks_.pop_back();

    Ref<Code> result{std::move(*builders_.back()).build()};
    builders_.pop_back();

    return result;
}

void CodeGen::declare_captures(const std::vector<OneCapture> &captures) {
    for (const OneCapture &capture : captures) {
        if (capture.capture_type_ == OneCapture::CaptureType::Reference) {
            // 引用捕获不产生任何指令，只在名字表上留个标记
            code().mark_ref_captured(code().intern_name(capture.identifier_));
        } else {
            code().add_value_captured(capture.identifier_);
        }
    }
}

void CodeGen::gen_capture_values(const std::vector<OneCapture> &captures, const Position pos) {
    std::uint32_t count{0};

    for (const OneCapture &capture : captures) {
        if (capture.capture_type_ == OneCapture::CaptureType::Reference) continue;

        // 写了 "= expr" 就求那个表达式，否则取同名的值
        if (capture.value_expr_)
            gen(capture.value_expr_, true);
        else
            code().emit(OpCode::LOAD_NAME, code().intern_name(capture.identifier_), pos);
        ++count;
    }

    code().emit(OpCode::MAKE_TUPLE, count, pos);
}

void CodeGen::gen_decorators_before(const std::vector<AstNodePtr> &decorators) {
    // 从前到后求值，离 func / class 越远的越先
    for (const AstNodePtr &decorator : decorators) gen(decorator, true);
}

void CodeGen::gen_decorators_after(const std::vector<AstNodePtr> &decorators, const Position pos) {
    // 由近到远逐个调用，一个装饰器一条 CALL
    for (std::size_t i{0}; i < decorators.size(); ++i) code().emit(OpCode::CALL, 1, pos);
}

// ============================================================
// 字面量
// ============================================================

void CodeGen::visit(const AstNodeLiteralNone &node) {
    if (want_value_) gen_load_const(literal_object(node), node.pos_);
}

void CodeGen::visit(const AstNodeLiteralEllipsis &node) {
    if (want_value_) gen_load_const(literal_object(node), node.pos_);
}

void CodeGen::visit(const AstNodeLiteralBool &node) {
    if (want_value_) gen_load_const(literal_object(node), node.pos_);
}

void CodeGen::visit(const AstNodeLiteralInt &node) {
    if (want_value_) gen_load_const(literal_object(node), node.pos_);
}

void CodeGen::visit(const AstNodeLiteralDecimal &node) {
    if (!want_value_) return;

    const ObjectRef value{literal_object(node)};
    if (!value) error_internal("malformed decimal literal", node.pos_);

    gen_load_const(value, node.pos_);
}

void CodeGen::visit(const AstNodeLiteralStr &node) {
    if (want_value_) gen_load_const(literal_object(node), node.pos_);
}

void CodeGen::visit(const AstNodeLiteralGL &node) {
    // 压的是 _L / _G 那个字典本身，不是视图也不是快照
    if (want_value_)
        code().emit(
            OpCode::LOAD_GL, node.value_ == AstNodeLiteralGL::GLType::G ? 1u : 0u, node.pos_
        );
}

void CodeGen::visit(const AstNodeLiteralTuple &node) {
    const bool want{want_value_};

    // 元组不可变，元素全是常量时整个进常量表，一条 LOAD_CONST 就够
    if (const ObjectRef value{literal_object(node)}) {
        if (want) gen_load_const(value, node.pos_);

        return;
    }

    gen_sequence(node.items_, true, node.pos_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeLiteralList &node) {
    const bool want{want_value_};

    // list 可变，每次求值都得是新对象，元素全是常量也不例外
    gen_sequence(node.items_, false, node.pos_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeLiteralDict &node) {
    const bool want{want_value_};

    gen_dict(node.items_, node.pos_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

// ============================================================
// 多条表达式
// ============================================================

void CodeGen::visit(const AstNodeProgram &node) {
    // Program 只出现在文件 / 函数体 / 类体三处，各自走 gen_program，不会当子表达式分派到这里
    error_internal("a program cannot appear as a sub-expression", node.pos_);
}

void CodeGen::visit(const AstNodeCompound &node) { gen_exprs(node.exprs_, want_value_, node.pos_); }

// ============================================================
// 运算符
// ============================================================

void CodeGen::visit(const AstNodeStar &node) {
    error_internal("* is only allowed in containers, call arguments and destructuring", node.pos_);
}

void CodeGen::visit(const AstNodeDoubleStar &node) {
    error_internal("** is only allowed in dict literals and call arguments", node.pos_);
}

void CodeGen::visit(const AstNodeOpUnary &node) {
    const bool want{want_value_};

    gen(node.operand_, true);
    code().emit(OpCode::UNARY_OP, to_unary_op(node.op_), node.pos_op_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeOpBinary &node) {
    const bool want{want_value_};
    const Position pos{node.pos_op_};

    const bool is_and{node.op_ == AstNodeOpBinary::OpType::And};
    const bool is_or{node.op_ == AstNodeOpBinary::OpType::Or};

    if (!is_and && !is_or) {
        gen(node.left_, true);
        gen(node.right_, true);
        code().emit(OpCode::BINARY_OP, to_binary_op(node.op_), pos);
        if (!want) code().emit(OpCode::POP_TOP, node.pos_);

        return;
    }

    // and / or 不落在 BINARY_OP 上，只用消费型的 JUMP_IF_FALSE 短路。
    // want 时先复制一份去做真值测试，原件留在栈上当短路的结果
    const CodeBuilder::Label end{code().new_label()};

    gen(node.left_, true);
    if (want) code().emit(OpCode::COPY, 1, pos);
    code().emit(OpCode::TO_BOOL, pos);

    if (is_and) {
        code().emit_jump(OpCode::JUMP_IF_FALSE, end, pos);
        if (want) code().emit(OpCode::POP_TOP, pos);
        gen(node.right_, want);
    } else {
        // 没有 JUMP_IF_TRUE：把"假才跳"倒过来，再加一条无条件跳
        const CodeBuilder::Label rhs{code().new_label()};

        code().emit_jump(OpCode::JUMP_IF_FALSE, rhs, pos);
        code().emit_jump(OpCode::JUMP, end, pos);
        code().place_label(rhs);
        if (want) code().emit(OpCode::POP_TOP, pos);
        gen(node.right_, want);
    }

    code().place_label(end);
}

void CodeGen::visit(const AstNodeCompare &node) {
    const bool want{want_value_};

    if (node.operands_.size() != node.ops_.size() + 1 ||
        node.positions_op_.size() != node.ops_.size())
        error_internal("malformed comparison chain", node.pos_);

    std::vector<ChainStep> steps;
    steps.reserve(node.ops_.size());
    for (std::size_t i{0}; i < node.ops_.size(); ++i)
        steps.push_back(ChainStep{to_binary_op(node.ops_[i]), node.positions_op_[i]});

    gen_chain(node.operands_, steps, want);
}

void CodeGen::visit(const AstNodeIs &node) {
    const bool want{want_value_};

    if (node.positions_op_.size() + 1 != node.operands_.size())
        error_internal("malformed is chain", node.pos_);

    std::vector<ChainStep> steps;
    steps.reserve(node.positions_op_.size());
    for (const Position pos : node.positions_op_) steps.push_back(ChainStep{BinaryOp::Is, pos});

    gen_chain(node.operands_, steps, want);
}

void CodeGen::gen_chain(
    const std::vector<AstNodePtr> &operands, const std::vector<ChainStep> &steps, const bool want
) {
    const Position pos{operands.front()->pos_};

    gen(operands[0], true);
    gen(operands[1], true);

    // 两个操作数就没有链可言
    if (steps.size() == 1) {
        code().emit(OpCode::BINARY_OP, steps[0].op_, steps[0].pos_);
        if (!want) code().emit(OpCode::POP_TOP, pos);

        return;
    }

    const CodeBuilder::Label fail{code().new_label()};
    const CodeBuilder::Label end{code().new_label()};

    // 中间段：右操作数复制一份垫到左操作数下面，留给下一段当左操作数。
    // 比较结果不保证是 bool（用户重载的 __op_lt__ 可以返回别的），测试前垫一条 TO_BOOL
    for (std::size_t i{0}; i + 1 < steps.size(); ++i) {
        code().emit(OpCode::COPY, 1, steps[i].pos_);
        code().emit(OpCode::INSERT, 3, steps[i].pos_);
        code().emit(OpCode::BINARY_OP, steps[i].op_, steps[i].pos_);

        code().emit(OpCode::COPY, 1, steps[i].pos_);
        code().emit(OpCode::TO_BOOL, steps[i].pos_);
        code().emit_jump(OpCode::JUMP_IF_FALSE, fail, steps[i].pos_);
        code().emit(OpCode::POP_TOP, steps[i].pos_);

        gen(operands[i + 2], true);
    }

    // 末段不用再留操作数
    code().emit(OpCode::BINARY_OP, steps.back().op_, steps.back().pos_);
    code().emit_jump(OpCode::JUMP, end, pos);

    // 所有中间段的失败分支共享这里：栈上是 [左操作数, 结果]，换个个儿把左操作数弹掉
    code().place_label(fail);
    code().emit(OpCode::INSERT, 2, pos);
    code().emit(OpCode::POP_TOP, pos);

    code().place_label(end);
    if (!want) code().emit(OpCode::POP_TOP, pos);
}

// ============================================================
// 赋值
// ============================================================

void CodeGen::visit(const AstNodeAssign &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    const std::uint32_t prefix{gen_target_prefix(*node.target_, false)};
    gen(node.value_, true);

    // want 时垫一份出来，让写入指令把原件弹光
    if (want) {
        code().emit(OpCode::COPY, 1, pos);
        if (prefix > 0) code().emit(OpCode::INSERT, prefix + 2, pos);
    }

    gen_target_store(*node.target_);
}

void CodeGen::visit(const AstNodeCompoundAssign &node) {
    const bool want{want_value_};
    const Position pos{node.pos_op_};

    // 目标只求值一次，读旧值靠复制已经在栈上的那份
    const std::uint32_t prefix{gen_target_prefix(*node.target_, false)};
    gen_target_load(*node.target_, prefix);
    gen(node.value_, true);
    code().emit(OpCode::BINARY_OP, to_binary_op(node.op_), pos);

    if (want) {
        code().emit(OpCode::COPY, 1, pos);
        if (prefix > 0) code().emit(OpCode::INSERT, prefix + 2, pos);
    }

    gen_target_store(*node.target_);
}

// ============================================================
// 后缀表达式
// ============================================================

void CodeGen::visit(const AstNodeCall &node) {
    const bool want{want_value_};

    gen(node.object_, true);
    gen_call(node.args_, node.args_.pos_paren_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeIndex &node) {
    const bool want{want_value_};
    const Position pos{node.pos_bracket_};

    gen(node.object_, true);

    // 下标里出现 *expr 时个数不再是编译期常数，先拼成一个 list
    if (has_expansion(node.args_)) {
        gen_sequence(node.args_, false, pos);
        code().emit(OpCode::GET_INDEX_EX, pos);
    } else {
        for (const AstNodePtr &arg : node.args_) gen(arg, true);
        code().emit(OpCode::GET_INDEX, static_cast<std::uint32_t>(node.args_.size()), pos);
    }

    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeAttr &node) {
    const bool want{want_value_};

    gen(node.object_, true);
    code().emit(OpCode::GET_ATTR, code().intern_name(node.attr_), node.pos_dot_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

// ============================================================
// 名字
// ============================================================

void CodeGen::visit(const AstNodeIdentifier &node) {
    const bool want{want_value_};

    // 读不到就是 NameError，是指令固有行为，没人要值也照发
    code().emit(OpCode::LOAD_NAME, code().intern_name(node.identifier_), node.pos_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeDel &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (const auto *const name{dynamic_cast<const AstNodeIdentifier *>(node.target_.get())}) {
        code().emit(OpCode::DELETE_NAME, code().intern_name(name->identifier_), pos);
    } else if (const auto *const attr{dynamic_cast<const AstNodeAttr *>(node.target_.get())}) {
        gen(attr->object_, true);
        code().emit(OpCode::DELETE_ATTR, code().intern_name(attr->attr_), attr->pos_dot_);
    } else {
        error_internal("del target must be an identifier or an attribute access", pos);
    }

    if (want) gen_none(pos);
}

void CodeGen::visit(const AstNodeGlobal &node) {
    const bool want{want_value_};

    code().emit(OpCode::DECLARE_GLOBAL, code().intern_name(node.identifier_), node.pos_);
    if (want) gen_none(node.pos_);
}

// ============================================================
// 控制流
// ============================================================

void CodeGen::visit(const AstNodeIf &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (node.clauses_.empty()) error_internal("if without any clause", pos);

    const CodeBuilder::Label end{code().new_label()};

    for (const AstNodeIf::AstNodeCondAndExpr &clause : node.clauses_) {
        const CodeBuilder::Label next{code().new_label()};

        gen_jump_if_false(*clause.cond_, next);
        gen(clause.body_, want);
        code().emit_jump(OpCode::JUMP, end, pos);
        code().place_label(next);
    }

    // 缺 else 时补的那个 None 是 codegen 自己加的，没人要值就不必补
    if (node.else_expr_)
        gen(node.else_expr_, want);
    else if (want)
        gen_none(pos);

    code().place_label(end);
}

void CodeGen::visit(const AstNodeForCond &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};
    const bool counting{node.collect_.container_ == CollectMark::Container::None};

    // 结果槽整个循环期间待在栈上，结束时就是整条表达式的值
    const bool has_slot{gen_loop_slot(node.collect_, want, pos)};
    const std::uint32_t base{code().stack_depth()};

    if (node.init_) gen(node.init_, false);

    const CodeBuilder::Label loop{code().new_label()};
    const CodeBuilder::Label inc{code().new_label()};
    const CodeBuilder::Label end{code().new_label()};
    const CodeBuilder::Label count_continue{code().new_label()};

    code().place_label(loop);
    // cond 为空视为 True
    if (node.cond_) gen_jump_if_false(*node.cond_, end);

    loops_.push_back(LoopContext{end, counting ? count_continue : inc, base, base, blocks_.size()});
    // 循环体恒按 want_value=true 编译，每轮的值都要被收尾逻辑读一次
    gen(node.body_, true);
    gen_loop_collect(node.collect_, has_slot, 1, count_continue, pos);
    loops_.pop_back();

    code().place_label(inc);
    if (node.inc_) gen(node.inc_, false);
    code().emit_jump(OpCode::JUMP, loop, pos);

    code().place_label(end);
    if (has_slot && !want) code().emit(OpCode::POP_TOP, pos);
}

void CodeGen::visit(const AstNodeForIter &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};
    const bool counting{node.collect_.container_ == CollectMark::Container::None};

    const bool has_slot{gen_loop_slot(node.collect_, want, pos)};
    const std::uint32_t after_slot{code().stack_depth()};

    gen(node.iterable_, true);
    code().emit(OpCode::GET_ITER, node.iterable_->pos_);

    // 栈序只能是"结果槽在下、迭代器在上"：FOR_ITER 要求迭代器在栈顶，而结果槽要活过整个循环
    const std::uint32_t base{code().stack_depth()};

    const CodeBuilder::Label loop{code().new_label()};
    const CodeBuilder::Label end{code().new_label()};
    const CodeBuilder::Label count_continue{code().new_label()};

    code().place_label(loop);
    code().emit_jump(OpCode::FOR_ITER, end, pos);

    if (node.target_)
        gen_store_one(*node.target_);
    else
        code().emit(OpCode::POP_TOP, pos);

    loops_.push_back(
        LoopContext{end, counting ? count_continue : loop, after_slot, base, blocks_.size()}
    );
    gen(node.body_, true);
    // 每轮的值压在迭代器上面，收尾指令要跨过它才够得着结果槽
    gen_loop_collect(node.collect_, has_slot, 2, count_continue, pos);
    loops_.pop_back();

    code().emit_jump(OpCode::JUMP, loop, pos);

    code().place_label(end);
    if (has_slot && !want) code().emit(OpCode::POP_TOP, pos);
}

void CodeGen::visit(const AstNodeBreak &node) {
    if (loops_.empty()) error_internal("break outside a loop", node.pos_);

    const LoopContext loop{loops_.back()};
    gen_unwind(loop.block_depth_, loop.break_depth_, node.pos_);
    code().emit_jump(OpCode::JUMP, loop.break_label_, node.pos_);
}

void CodeGen::visit(const AstNodeContinue &node) {
    if (loops_.empty()) error_internal("continue outside a loop", node.pos_);

    const LoopContext loop{loops_.back()};
    gen_unwind(loop.block_depth_, loop.continue_depth_, node.pos_);
    code().emit_jump(OpCode::JUMP, loop.continue_label_, node.pos_);
}

void CodeGen::visit(const AstNodeReturn &node) {
    const Position pos{node.pos_};

    if (node.value_)
        gen(node.value_, true);
    else
        gen_none(pos);

    gen_unwind_with_value(pos);
    code().emit(OpCode::RETURN_VALUE, pos);
}

void CodeGen::visit(const AstNodeRaise &node) {
    gen(node.value_, true);
    code().emit(OpCode::RAISE, node.pos_);
}

void CodeGen::visit(const AstNodeTry &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};
    const std::uint32_t entry{code().stack_depth()};

    CodeBuilder::Label finally_body{};
    if (node.finally_expr_) {
        finally_body = code().new_label();
        code().emit_jump(OpCode::SETUP_FINALLY, finally_body, pos);
        blocks_.push_back(BlockContext{true, finally_body, entry});
    }

    if (node.except_clauses_.empty()) {
        gen(node.try_expr_, want);
    } else {
        const CodeBuilder::Label handlers{code().new_label()};
        const CodeBuilder::Label after{code().new_label()};

        code().emit_jump(OpCode::SETUP_EXCEPT, handlers, pos);
        blocks_.push_back(BlockContext{false, {}, entry});
        gen(node.try_expr_, want);
        blocks_.pop_back();
        code().emit(OpCode::POP_BLOCK, pos);
        code().emit_jump(OpCode::JUMP, after, pos);

        // 处理器入口栈上只有那个异常对象，CHECK_EXC_MATCH 会把它留下，不用额外复制
        code().place_label(handlers);
        for (const AstNodeTry::AstNodeExceptAndExpr &clause : node.except_clauses_) {
            const CodeBuilder::Label next{code().new_label()};
            const CodeBuilder::Label matched{code().new_label()};

            for (std::size_t i{0}; i < clause.exceptions_.size(); ++i) {
                gen(clause.exceptions_[i], true);
                code().emit(OpCode::CHECK_EXC_MATCH, pos);

                // 一条子句列了多个异常类型时，任一匹配就算中
                if (i + 1 < clause.exceptions_.size()) {
                    const CodeBuilder::Label try_next{code().new_label()};

                    code().emit_jump(OpCode::JUMP_IF_FALSE, try_next, pos);
                    code().emit_jump(OpCode::JUMP, matched, pos);
                    code().place_label(try_next);
                } else {
                    code().emit_jump(OpCode::JUMP_IF_FALSE, next, pos);
                }
            }

            code().place_label(matched);
            if (clause.target_)
                gen_store_one(*clause.target_);
            else
                code().emit(OpCode::POP_TOP, pos);

            gen(clause.body_, want);
            code().emit_jump(OpCode::JUMP, after, pos);
            code().place_label(next);
        }

        code().emit(OpCode::RERAISE, pos);
        code().place_label(after);
    }

    if (!node.finally_expr_) return;

    blocks_.pop_back();
    code().emit(OpCode::POP_BLOCK, pos);

    // 进 finally 体时栈顶恒是"这条路径要带出去的那个值"。正常路径带的是 try 的值，
    // 没人要值时就压个 None 占位，好让异常路径和这里的栈深对得上
    if (!want) gen_none(pos);
    code().emit_jump(OpCode::CALL_FINALLY, finally_body, pos);
    if (!want) code().emit(OpCode::POP_TOP, pos);

    const CodeBuilder::Label after_all{code().new_label()};
    code().emit_jump(OpCode::JUMP, after_all, pos);

    // finally 体只编译一份，正常路径和异常路径共用，末尾统一由 END_FINALLY 分派
    code().place_label(finally_body);
    gen(node.finally_expr_, false);
    code().emit(OpCode::END_FINALLY, pos);

    code().place_label(after_all);
}

// ============================================================
// 装饰器、eval、import
// ============================================================

void CodeGen::visit(const AstNodeDecorator &node) {
    const bool want{want_value_};

    // @d expr 就是 d(expr)
    gen(node.decorator_, true);
    gen(node.target_, true);
    code().emit(OpCode::CALL, 1, node.pos_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeEval &node) {
    const bool want{want_value_};

    // eval 不能像 import 那样包成一个内部函数：它要拿到调用帧的名字环境
    gen_call_args_packed(node.args_, node.args_.pos_paren_);
    code().emit(OpCode::EVAL, node.pos_);
    if (!want) code().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeImportKw &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (node.segments_.empty()) error_internal("import without a name", pos);

    std::u32string dotted;
    for (std::size_t i{0}; i < node.segments_.size(); ++i) {
        if (i > 0) dotted += U'.';
        dotted += node.segments_[i];
    }

    // 压入的是第一段的模块对象，它同时也是整条表达式的值
    code().emit(OpCode::IMPORT, code().intern_name(dotted), pos);
    if (want) code().emit(OpCode::COPY, 1, pos);
    code().emit(OpCode::STORE_NAME, code().intern_name(node.segments_.front()), pos);
}

void CodeGen::visit(const AstNodeImportCall &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    // 实参形状跟普通调用一致，所以加载算法包成一个内部函数对象接一次普通调用即可
    code().emit(OpCode::LOAD_INTERNAL, static_cast<std::uint32_t>(InternalObject::ImportFunc), pos);
    gen_call(node.args_, node.args_.pos_paren_);
    if (!want) code().emit(OpCode::POP_TOP, pos);
}

// ============================================================
// 函数与类
// ============================================================

void CodeGen::visit(const AstNodeFunc &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (!node.body_) error_internal("function without a body", pos);

    // 先把函数体编译成一份嵌套 Code
    push_scope();
    declare_captures(node.captures_);
    code().set_params(build_param_shape(node));
    gen_program(*node.body_);
    Ref<Code> body{pop_scope()};

    const std::uint32_t slot{code().add_nested_code(std::move(body))};

    gen_decorators_before(node.decorators_);
    gen_capture_values(node.captures_, pos);

    // 形参段：按声明顺序，每个形参写了注解就压注解、写了默认值就压默认值。
    // 哪一项对应谁由 Code::ParamShape 的位图还原
    std::uint32_t param_items{0};
    const auto push_params{[&](const std::vector<AstNodeFunc::OneParam> &params) {
        for (const AstNodeFunc::OneParam &param : params) {
            if (param.type_annotation_) {
                gen(param.type_annotation_, true);
                ++param_items;
            }
            if (param.default_value_) {
                gen(param.default_value_, true);
                ++param_items;
            }
        }
    }};
    push_params(node.params_.positional_);
    push_params(node.params_.kw_only_);
    code().emit(OpCode::MAKE_TUPLE, param_items, pos);

    gen_optional(node.return_type_, pos);
    gen_optional(node.doc_, pos);
    if (node.name_)
        gen_load_const(make_ref<Str>(*node.name_), pos);
    else
        gen_missing(pos);

    code().emit(OpCode::MAKE_FUNC, slot, pos);
    gen_decorators_after(node.decorators_, pos);

    // 命名函数绑定一次；匿名的没有名字可绑
    if (node.name_) {
        if (want) code().emit(OpCode::COPY, 1, pos);
        code().emit(OpCode::STORE_NAME, code().intern_name(*node.name_), pos);
    } else if (!want) {
        code().emit(OpCode::POP_TOP, pos);
    }
}

void CodeGen::visit(const AstNodeClass &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (!node.body_) error_internal("class without a body", pos);

    push_scope();
    declare_captures(node.captures_);
    gen_program(*node.body_);
    Ref<Code> body{pop_scope()};

    const std::uint32_t slot{code().add_nested_code(std::move(body))};

    gen_decorators_before(node.decorators_);

    // 基类元组顶掉函数那边的形参段。它排在捕获之前——类的捕获是在新建类体那一帧之后才处理的，
    // 而建帧要等基类求值完，跟函数那边捕获排最前正好相反
    for (const AstNodePtr &base : node.bases_) gen(base, true);
    code().emit(OpCode::MAKE_TUPLE, static_cast<std::uint32_t>(node.bases_.size()), pos);

    gen_capture_values(node.captures_, pos);

    gen_optional(node.doc_, pos);
    if (node.name_)
        gen_load_const(make_ref<Str>(*node.name_), pos);
    else
        gen_missing(pos);

    // 不直接产出类对象：新建局部帧执行类体，类体返回时才由类构建器收属性、算 MRO
    code().emit(OpCode::MAKE_CLASS, slot, pos);
    gen_decorators_after(node.decorators_, pos);

    if (node.name_) {
        if (want) code().emit(OpCode::COPY, 1, pos);
        code().emit(OpCode::STORE_NAME, code().intern_name(*node.name_), pos);
    } else if (!want) {
        code().emit(OpCode::POP_TOP, pos);
    }
}

Code::ParamShape CodeGen::build_param_shape(const AstNodeFunc &node) {
    const auto one{[](const AstNodeFunc::OneParam &param) {
        return Code::OneParamShape{
            make_ref<Str>(param.identifier_),
            param.default_value_ != nullptr,
            param.type_annotation_ != nullptr
        };
    }};

    Code::ParamShape shape;
    for (const AstNodeFunc::OneParam &param : node.params_.positional_)
        shape.positional.push_back(one(param));
    for (const AstNodeFunc::OneParam &param : node.params_.kw_only_)
        shape.kw_only.push_back(one(param));

    if (node.params_.var_args_name_) shape.var_args = make_ref<Str>(*node.params_.var_args_name_);
    if (node.params_.var_kwargs_name_)
        shape.var_kwargs = make_ref<Str>(*node.params_.var_kwargs_name_);
    shape.has_return_annotation = node.return_type_ != nullptr;

    return shape;
}

Ref<Code> CodeGen::generate_program(const AstNodeProgram &root, std::string file_path) {
    if (!Runtime::ready()) throw InternalError{"CodeGen: runtime is not initialized"};

    CodeGen generator{std::move(file_path)};
    generator.push_scope();
    generator.gen_program(root);

    return generator.pop_scope();
}
