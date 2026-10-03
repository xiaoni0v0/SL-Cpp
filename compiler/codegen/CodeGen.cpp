#include "CodeGen.h"

#include "../../common/Position.h"
#include "../../cppexceptions/InternalError.h"
#include "../../numeric/BigDec.h"
#include "../../numeric/BigInt.h"
#include "../../runtime/Object.h"
#include "../../runtime/Runtime.h"
#include "../../runtime/objects/Code.h"
#include "../../runtime/objects/Decimal.h"
#include "../../runtime/objects/Int.h"
#include "../../runtime/objects/Str.h"
#include "../../runtime/objects/Tuple.h"
#include "../../utils/string_utils.h"
#include "../parser/ast_nodes/ast_nodes.h"
#include "CodeBuilder.h"
#include "opcode.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

    throw InternalError{"CodeGen: no BinaryOp for and/or"};
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

    throw InternalError{"CodeGen: invalid compare op"};
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

    throw InternalError{"CodeGen: invalid unary op"};
}

// 字面量节点对应的 SL 对象。不是能进常量表的字面量就返回空
ObjectRef literal_object(const AstNode &node) {
    if (dynamic_cast<const AstNodeLiteralNone *>(&node))
        return ObjectRef{Runtime::singleton_none()};
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&node))
        return ObjectRef{Runtime::singleton_ellipsis()};
    if (const auto *flag{dynamic_cast<const AstNodeLiteralBool *>(&node)})
        return ObjectRef{Runtime::singleton_bool(flag->value_)};
    if (const auto *text{dynamic_cast<const AstNodeLiteralStr *>(&node)})
        return make_ref<Str>(text->value_);
    if (const auto *number{dynamic_cast<const AstNodeLiteralInt *>(&node)})
        return make_ref<Int>(BigInt::from_decimal_string(u32_to_utf8(number->raw_)));

    if (const auto *number{dynamic_cast<const AstNodeLiteralDecimal *>(&node)}) {
        // 构造不舍入、精确保留全部位数，所以不查上下文
        const std::optional value{BigDec::try_from_string(u32_to_utf8(number->raw_))};
        if (!value) return {};

        return make_ref<Decimal>(*value);
    }

    // 元组自底向上：元素全是常量才算，有一个不是整个就得现建
    if (const auto *tuple{dynamic_cast<const AstNodeLiteralTuple *>(&node)}) {
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

// items 里有没有 * / ** 展开项
bool has_expansion(const std::vector<AstNodePtr> &items) {
    return std::ranges::any_of(items, [](const AstNodePtr &item) -> bool {
        return dynamic_cast<const AstNodeStar *>(item.get()) != nullptr;
    });
}

// 解构目标的元素清单；不是解构目标就返回空
const std::vector<AstNodePtr> *destructuring_items(const AstNode &target) {
    if (const auto *tuple{dynamic_cast<const AstNodeLiteralTuple *>(&target)})
        return &tuple->items_;
    if (const auto *list{dynamic_cast<const AstNodeLiteralList *>(&target)}) return &list->items_;

    return nullptr;
}

// 形参形状。默认值与注解本身不在 Code 里，这里只记有没有
Code::ParamShape build_param_shape(const AstNodeFunc &node) {
    const auto one{[](const AstNodeFunc::OneParam &param) {
        return Code::OneParamShape{
            .name = make_ref<Str>(param.identifier_),
            .has_default = param.default_value_ != nullptr,
            .has_annotation = param.type_annotation_ != nullptr,
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

} // namespace

CodeGen::CodeGen(std::string file_path) : file_path_{std::move(file_path)} {}

CodeBuilder &CodeGen::builder() const {
    assert(!builders_.empty());

    return *builders_.top();
}

void CodeGen::error_internal(const std::string &msg, const Position pos) const {
    throw InternalError{file_path_, pos, "CodeGen: " + msg};
}

void CodeGen::gen(const AstNode &node, const bool want_value) {
    const std::uint32_t before{builder().stack_depth()};
    const bool was_reachable{!builder().unreachable()};

    want_value_ = want_value;
    node.accept(*this);

    // 每个节点编完，栈深恰好多 want_value 那么多。漏了就是一份悄悄错位的字节码，Release 下也要拦
    if (was_reachable && !builder().unreachable() &&
        builder().stack_depth() != before + (want_value ? 1u : 0u))
        error_internal("stack depth must change by exactly want_value", node.pos_);
}

void CodeGen::gen(const AstNodePtr &node, const bool want_value) {
    if (!node) error_internal("unexpected null node", Position{0, 0});

    gen(*node, want_value);
}

void CodeGen::gen_program(const AstNodeProgram &program) {
    for (const AstNodePtr &expr : program.exprs_) gen(expr, false);

    // 前边有 return
    if (builder().unreachable()) return;

    // 前边无 return，补上个 return None
    gen_none(program.pos_);
    builder().emit(OpCode::RETURN_VALUE, program.pos_);
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

void CodeGen::gen_load_const(const ObjectRef &value, const Position pos) const {
    builder().emit(OpCode::LOAD_CONST, builder().intern_const(value), pos);
}

void CodeGen::gen_none(const Position pos) const {
    gen_load_const(ObjectRef{Runtime::singleton_none()}, pos);
}

void CodeGen::gen_missing(const Position pos) const {
    builder().emit(OpCode::LOAD_INTERNAL, static_cast<std::uint32_t>(InternalObject::Missing), pos);
}

void CodeGen::gen_optional(const AstNodePtr &node, const Position pos) {
    if (node)
        gen(*node, true);
    else
        gen_missing(pos);
}

void CodeGen::gen_pop_to(const std::uint32_t depth, const Position pos) const {
    if (builder().unreachable()) return;

    if (builder().stack_depth() < depth) error_internal("cannot pop to a deeper stack", pos);
    while (builder().stack_depth() > depth) builder().emit(OpCode::POP_TOP, pos);
}

void CodeGen::gen_jump_if_false(const AstNode &cond, const CodeBuilder::Label target) {
    gen(cond, true);
    builder().emit(OpCode::TO_BOOL, cond.pos_);
    builder().emit_jump(OpCode::JUMP_IF_FALSE, target, cond.pos_);
}

void CodeGen::gen_unwind(
    const std::size_t to_block_depth, const std::uint32_t target_depth, const Position pos
) const {
    // 由内到外逐层退出。块栈是运行期的，跳走之前必须一层层弹干净，否则跳到循环外之后
    // 再抛异常，还会撞进这些已经失效的处理器
    for (std::size_t i{blocks_.size()}; i > to_block_depth && !builder().unreachable(); --i) {
        const BlockContext &block{blocks_[i - 1]};

        builder().emit(OpCode::POP_BLOCK, pos);
        if (!block.is_finally_) continue;

        // 进 finally 体时栈顶恒是"这条路径要带出去的那个值"。break/continue 没有值可带，
        // 压个 None 占位，回来再弹掉
        gen_pop_to(block.entry_depth_, pos);
        gen_none(pos);
        builder().emit_jump(OpCode::CALL_FINALLY, block.finally_label_, pos);
        builder().emit(OpCode::POP_TOP, pos);
    }

    gen_pop_to(target_depth, pos);
}

void CodeGen::gen_unwind_with_value(const Position pos) const {
    // 返回值就是这条路径要带出去的那个值，本来就在栈顶
    for (std::size_t i{blocks_.size()}; i > 0 && !builder().unreachable(); --i) {
        const BlockContext &block{blocks_[i - 1]};

        // 自己这层块得先弹掉，否则 finally 体里抛的异常会被它自己接住
        builder().emit(OpCode::POP_BLOCK, pos);
        if (!block.is_finally_) continue;

        // 把返回值沉下去，弹掉夹在中间的临时值
        if (builder().stack_depth() <= block.entry_depth_)
            error_internal("return value is below the try entry depth", pos);
        if (const std::uint32_t extra{builder().stack_depth() - block.entry_depth_ - 1};
            extra > 0) {
            builder().emit(OpCode::INSERT, extra + 1, pos);
            for (std::uint32_t k{0}; k < extra; ++k) builder().emit(OpCode::POP_TOP, pos);
        }
        builder().emit_jump(OpCode::CALL_FINALLY, block.finally_label_, pos);
    }
}

void CodeGen::gen_sequence(
    const std::vector<AstNodePtr> &items, const bool as_tuple, const Position pos
) {
    const auto count{static_cast<std::uint32_t>(items.size())};

    // 个数是编译期常数，一条指令建好，不必逐项并入
    if (!has_expansion(items)) {
        for (const AstNodePtr &item : items) gen(item, true);
        builder().emit(as_tuple ? OpCode::MAKE_TUPLE : OpCode::MAKE_LIST, count, pos);

        return;
    }

    // 第一个展开项之前的照常逐个求值，一次建好；其后逐项并入。求值顺序与源码一致
    std::size_t head{0};
    while (head < items.size() && !dynamic_cast<const AstNodeStar *>(items[head].get())) ++head;

    for (std::size_t i{0}; i < head; ++i) gen(items[i], true);
    builder().emit(OpCode::MAKE_LIST, static_cast<std::uint32_t>(head), pos);

    for (std::size_t i{head}; i < items.size(); ++i) {
        const auto *star{dynamic_cast<const AstNodeStar *>(items[i].get())};
        gen(star ? star->operand_ : items[i], true);
        builder().emit(star ? OpCode::LIST_EXTEND : OpCode::LIST_APPEND, 1, items[i]->pos_);
    }

    if (as_tuple) builder().emit(OpCode::LIST_TO_TUPLE, pos);
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
        builder().emit(OpCode::MAKE_DICT, static_cast<std::uint32_t>(items.size()), pos);

        return;
    }

    for (std::size_t i{0}; i < head; ++i) {
        gen(items[i].first, true);
        gen(items[i].second, true);
    }
    builder().emit(OpCode::MAKE_DICT, static_cast<std::uint32_t>(head), pos);

    for (std::size_t i{head}; i < items.size(); ++i) {
        const auto &[key, value]{items[i]};

        if (const auto *spread{dynamic_cast<const AstNodeDoubleStar *>(key.get())}) {
            gen(spread->operand_, true);
            builder().emit(OpCode::DICT_MERGE, 1, key->pos_);
        } else {
            gen(key, true);
            gen(value, true);
            builder().emit(OpCode::DICT_PUT, 1, key->pos_);
        }
    }
}

void CodeGen::gen_chain(
    const std::vector<AstNodePtr> &operands, const std::vector<ChainStep> &steps, const bool want
) {
    gen(operands[0], true);
    gen(operands[1], true);

    const Position pos{operands.front()->pos_};

    // 两个操作数就没有链可言
    if (steps.size() == 1) {
        builder().emit(OpCode::BINARY_OP, steps[0].op_, steps[0].pos_);
        if (!want) builder().emit(OpCode::POP_TOP, pos);

        return;
    }

    const CodeBuilder::Label fail{builder().new_label()};
    const CodeBuilder::Label end{builder().new_label()};

    // 中间段：右操作数复制一份垫到左操作数下面，留给下一段当左操作数。
    // 比较结果不保证是 bool（用户重载的 __op_lt__ 可以返回别的），测试前垫一条 TO_BOOL
    for (std::size_t i{0}; i + 1 < steps.size(); ++i) {
        builder().emit(OpCode::COPY, 1, steps[i].pos_);
        builder().emit(OpCode::INSERT, 3, steps[i].pos_);
        builder().emit(OpCode::BINARY_OP, steps[i].op_, steps[i].pos_);

        builder().emit(OpCode::COPY, 1, steps[i].pos_);
        builder().emit(OpCode::TO_BOOL, steps[i].pos_);
        builder().emit_jump(OpCode::JUMP_IF_FALSE, fail, steps[i].pos_);
        builder().emit(OpCode::POP_TOP, steps[i].pos_);

        gen(operands[i + 2], true);
    }

    // 末段不用再留操作数
    builder().emit(OpCode::BINARY_OP, steps.back().op_, steps.back().pos_);
    builder().emit_jump(OpCode::JUMP, end, pos);

    // 所有中间段的失败分支共享这里：栈上是 [左操作数, 结果]，换个个儿把左操作数弹掉
    builder().place_label(fail);
    builder().emit(OpCode::INSERT, 2, pos);
    builder().emit(OpCode::POP_TOP, pos);

    builder().place_label(end);
    if (!want) builder().emit(OpCode::POP_TOP, pos);
}

std::uint32_t CodeGen::gen_target_prefix(const AstNode &target, const bool under_value) {
    // under_value 时待写入的值已经在栈顶，每压一项就把它沉到值下面
    const auto push{[&](const AstNodePtr &node) {
        gen(node, true);
        if (under_value) builder().emit(OpCode::INSERT, 2, node->pos_);
    }};

    if (const auto *attr{dynamic_cast<const AstNodeAttr *>(&target)}) {
        push(attr->object_);

        return 1;
    }

    if (const auto *index{dynamic_cast<const AstNodeIndex *>(&target)}) {
        push(index->object_);

        // 下标里出现 *expr 时个数不再是编译期常数，先拼成一个 list
        if (has_expansion(index->args_)) {
            gen_sequence(index->args_, false, index->pos_bracket_);
            if (under_value) builder().emit(OpCode::INSERT, 2, index->pos_bracket_);

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

    if (const auto *name{dynamic_cast<const AstNodeIdentifier *>(&target)}) {
        builder().emit(OpCode::STORE_NAME, builder().intern_name(name->identifier_), pos);

        return;
    }

    if (const auto *attr{dynamic_cast<const AstNodeAttr *>(&target)}) {
        builder().emit(OpCode::SET_ATTR, builder().intern_name(attr->attr_), attr->pos_dot_);

        return;
    }

    if (const auto *index{dynamic_cast<const AstNodeIndex *>(&target)}) {
        if (has_expansion(index->args_))
            builder().emit(OpCode::SET_INDEX_EX, index->pos_bracket_);
        else
            builder().emit(
                OpCode::SET_INDEX,
                static_cast<std::uint32_t>(index->args_.size()),
                index->pos_bracket_
            );

        return;
    }

    const std::vector<AstNodePtr> *items{destructuring_items(target)};
    if (!items) error_internal("cannot assign to this expression", pos);

    std::size_t star{items->size()};
    for (std::size_t i{0}; i < items->size(); ++i)
        if (dynamic_cast<const AstNodeStar *>((*items)[i].get()) != nullptr) star = i;

    if (star == items->size()) {
        builder().emit(OpCode::UNPACK, static_cast<std::uint32_t>(items->size()), pos);
    } else {
        // 星号左右两侧的项数各占参数里的一个字节。SemanticChecker 已经挡过，这里再拦一道：
        // 溢出的话高位会串进另一侧，悄悄编出错的解构
        const std::size_t after{items->size() - star - 1};
        if (star > kMaxInlineArg || after > kMaxInlineArg)
            error_internal("too many items around * in destructuring", pos);

        builder().emit(
            OpCode::UNPACK_EX,
            (static_cast<std::uint32_t>(after) << 8u) | static_cast<std::uint32_t>(star),
            pos
        );
    }

    // UNPACK 逆序压入，于是第一个元素在栈顶
    for (const AstNodePtr &item : *items) {
        const auto *item_star{dynamic_cast<const AstNodeStar *>(item.get())};
        gen_store_one(item_star ? *item_star->operand_ : *item);
    }
}

void CodeGen::gen_store_one(const AstNode &target) {
    gen_target_prefix(target, true);
    gen_target_store(target);
}

void CodeGen::gen_target_load(const AstNode &target, const std::uint32_t prefix) const {
    const Position pos{target.pos_};

    if (const auto *name{dynamic_cast<const AstNodeIdentifier *>(&target)}) {
        // 复合赋值的读写必须落在同一帧，所以不走逐级退回的那条
        builder().emit(OpCode::LOAD_NAME_STRICT, builder().intern_name(name->identifier_), pos);

        return;
    }

    // 前置操作数只求值一次，读旧值靠把它们整体复制一份
    for (std::uint32_t i{0}; i < prefix; ++i) builder().emit(OpCode::COPY, prefix, pos);

    if (const auto *attr{dynamic_cast<const AstNodeAttr *>(&target)}) {
        builder().emit(OpCode::GET_ATTR, builder().intern_name(attr->attr_), attr->pos_dot_);

        return;
    }

    const auto *index{dynamic_cast<const AstNodeIndex *>(&target)};
    if (!index) error_internal("cannot compound-assign to this expression", pos);

    if (has_expansion(index->args_))
        builder().emit(OpCode::GET_INDEX_EX, index->pos_bracket_);
    else
        builder().emit(
            OpCode::GET_INDEX, static_cast<std::uint32_t>(index->args_.size()), index->pos_bracket_
        );
}

void CodeGen::gen_call(const CallArgs &args, const Position pos) {
    const bool has_double_star{
        std::ranges::any_of(args.keyword_args_, [](const OneKwArg &arg) -> bool {
            return arg.kind_ == OneKwArg::Kind::DoubleStar;
        })
    };

    // 三条调用指令是按出现频率分的档：CALL 零额外对象，CALL_KW 多一个常量表里的名字元组，
    // CALL_EX 才建两个新容器
    if (has_expansion(args.positional_args_) || has_double_star) {
        gen_call_args_packed(args, pos);
        builder().emit(OpCode::CALL_EX, pos);

        return;
    }

    for (const AstNodePtr &arg : args.positional_args_) gen(arg, true);

    if (args.keyword_args_.empty()) {
        builder().emit(OpCode::CALL, static_cast<std::uint32_t>(args.positional_args_.size()), pos);

        return;
    }

    std::vector<ObjectRef> names;
    names.reserve(args.keyword_args_.size());
    for (const OneKwArg &arg : args.keyword_args_) {
        gen(arg.value_, true);
        names.emplace_back(make_ref<Str>(arg.keyword_));
    }

    gen_load_const(make_ref<Tuple>(std::move(names)), pos);
    builder().emit(
        OpCode::CALL_KW,
        static_cast<std::uint32_t>(args.positional_args_.size() + args.keyword_args_.size()),
        pos
    );
}

void CodeGen::gen_call_args_packed(const CallArgs &args, const Position pos) {
    gen_sequence(args.positional_args_, false, pos);

    if (args.keyword_args_.empty()) {
        builder().emit(OpCode::MAKE_DICT, 0, pos);

        return;
    }

    // 关键字组：identifier = expr 走 DICT_PUT，**expr 走 DICT_MERGE
    builder().emit(OpCode::MAKE_DICT, 0, pos);
    for (const OneKwArg &arg : args.keyword_args_) {
        if (arg.kind_ == OneKwArg::Kind::DoubleStar) {
            // 这一档的 value_ 存的是整个 AstNodeDoubleStar，要展开的是它的操作数
            const auto *spread{dynamic_cast<const AstNodeDoubleStar *>(arg.value_.get())};
            if (!spread) error_internal("malformed ** argument", pos);

            gen(spread->operand_, true);
            builder().emit(OpCode::DICT_MERGE, 1, pos);
        } else {
            gen_load_const(make_ref<Str>(arg.keyword_), pos);
            gen(arg.value_, true);
            builder().emit(OpCode::DICT_PUT, 1, pos);
        }
    }
}

bool CodeGen::gen_loop_slot(
    const CollectMark collect, const bool want_value, const Position pos
) const {
    switch (collect.container_) {
    case CollectMark::Container::None:
        // 计数模式的结果槽是纯 codegen 垫的，没人要值就退化成裸循环
        if (!want_value) return false;
        gen_load_const(make_ref<Int>(BigInt{0}), pos);

        return true;
    case CollectMark::Container::List:
        // 收尾指令自己可能抛 TypeError/ValueError，跳过就是悄悄吞异常，所以省不掉
        builder().emit(OpCode::MAKE_LIST, 0, pos);

        return true;
    case CollectMark::Container::Dict:
        builder().emit(OpCode::MAKE_DICT, 0, pos);

        return true;
    }

    error_internal("invalid collect container", pos);
}

void CodeGen::gen_loop_collect(
    const CollectMark collect, const bool has_slot, const std::uint32_t slot_depth,
    const CodeBuilder::Label count_continue, const Position pos
) const {
    switch (collect.container_) {
    case CollectMark::Container::None:
        // 本轮的值用不上，弹掉。continue 跳到 POP_TOP 之后：它自己的裁栈已经清掉那个值了
        builder().emit(OpCode::POP_TOP, pos);
        builder().place_label(count_continue);
        if (has_slot) builder().emit(OpCode::COUNT_INC, slot_depth, pos);

        return;
    case CollectMark::Container::List:
        builder().emit(
            collect.expand_ ? OpCode::LIST_EXTEND : OpCode::LIST_APPEND, slot_depth, pos
        );

        return;
    case CollectMark::Container::Dict:
        if (collect.expand_) {
            builder().emit(OpCode::DICT_MERGE, slot_depth, pos);
        } else {
            // UNPACK 2 给出 v k，DICT_PUT 要 d k v
            builder().emit(OpCode::UNPACK, 2, pos);
            builder().emit(OpCode::INSERT, 2, pos);
            builder().emit(OpCode::DICT_PUT, slot_depth, pos);
        }

        return;
    }

    error_internal("invalid collect container", pos);
}

void CodeGen::begin_code() {
    builders_.push(std::make_unique<CodeBuilder>(file_path_));

    // 隐藏外部块
    saved_loops_.push(std::exchange(loops_, {}));
    saved_blocks_.push(std::exchange(blocks_, {}));
}

Ref<Code> CodeGen::end_code() {
    loops_ = std::move(saved_loops_.top());
    saved_loops_.pop();
    blocks_ = std::move(saved_blocks_.top());
    saved_blocks_.pop();

    Ref result{std::move(*builders_.top()).build()};
    builders_.pop();

    return result;
}

void CodeGen::declare_captures(const std::vector<OneCapture> &captures) const {
    for (const OneCapture &capture : captures) {
        if (capture.capture_type_ == OneCapture::CaptureType::Reference) {
            // 引用捕获不产生任何指令，只在名字表上留个标记
            builder().mark_ref_captured(builder().intern_name(capture.identifier_));
        } else {
            builder().add_value_captured(capture.identifier_);
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
            builder().emit(OpCode::LOAD_NAME, builder().intern_name(capture.identifier_), pos);
        ++count;
    }

    builder().emit(OpCode::MAKE_TUPLE, count, pos);
}

void CodeGen::gen_decorators_eval(const std::vector<AstNodePtr> &decorators) {
    // 从前到后求值，离 func / class 越远的越先
    for (const AstNodePtr &decorator : decorators) gen(decorator, true);
}

void CodeGen::gen_decorators_apply(
    const std::vector<AstNodePtr> &decorators, const Position pos
) const {
    // 由近到远逐个调用，一个装饰器一条 CALL
    for (std::size_t i{0}; i < decorators.size(); ++i) builder().emit(OpCode::CALL, 1, pos);
}

void CodeGen::visit(const AstNodeClass &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (!node.body_) error_internal("class without a body", pos);

    begin_code();
    declare_captures(node.captures_);
    gen_program(*node.body_);
    Ref body{end_code()};

    const std::uint32_t slot{builder().add_nested_code(std::move(body))};

    gen_decorators_eval(node.decorators_);

    // 基类元组顶掉函数那边的形参段。它排在捕获之前——类的捕获是在新建类体那一帧之后才处理的，
    // 而建帧要等基类求值完，跟函数那边捕获排最前正好相反
    for (const AstNodePtr &base : node.bases_) gen(base, true);
    builder().emit(OpCode::MAKE_TUPLE, static_cast<std::uint32_t>(node.bases_.size()), pos);

    gen_capture_values(node.captures_, pos);

    gen_optional(node.doc_, pos);
    if (node.name_)
        gen_load_const(make_ref<Str>(*node.name_), pos);
    else
        gen_missing(pos);

    // 不直接产出类对象：新建局部帧执行类体，类体返回时才由类构建器收属性、算 MRO
    builder().emit(OpCode::MAKE_CLASS, slot, pos);
    gen_decorators_apply(node.decorators_, pos);

    if (node.name_) {
        if (want) builder().emit(OpCode::COPY, 1, pos);
        builder().emit(OpCode::STORE_NAME, builder().intern_name(*node.name_), pos);
    } else if (!want) {
        builder().emit(OpCode::POP_TOP, pos);
    }
}

void CodeGen::visit(const AstNodeIf &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (node.clauses_.empty()) error_internal("if without any clause", pos);

    const CodeBuilder::Label end{builder().new_label()};

    for (const AstNodeIf::AstNodeCondAndExpr &clause : node.clauses_) {
        const CodeBuilder::Label next{builder().new_label()};

        gen_jump_if_false(*clause.cond_, next);
        gen(clause.body_, want);
        builder().emit_jump(OpCode::JUMP, end, pos);
        builder().place_label(next);
    }

    // 缺 else 时补的那个 None 是 codegen 自己加的，没人要值就不必补
    if (node.else_expr_)
        gen(node.else_expr_, want);
    else if (want)
        gen_none(pos);

    builder().place_label(end);
}

void CodeGen::visit(const AstNodeForCond &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};
    const bool counting{node.collect_.container_ == CollectMark::Container::None};

    // 结果槽整个循环期间待在栈上，结束时就是整条表达式的值
    const bool has_slot{gen_loop_slot(node.collect_, want, pos)};
    const std::uint32_t base{builder().stack_depth()};

    if (node.init_) gen(node.init_, false);

    const CodeBuilder::Label loop{builder().new_label()};
    const CodeBuilder::Label inc{builder().new_label()};
    const CodeBuilder::Label end{builder().new_label()};
    const CodeBuilder::Label count_continue{builder().new_label()};

    builder().place_label(loop);
    // cond 为空视为 True
    if (node.cond_) gen_jump_if_false(*node.cond_, end);

    loops_.push(
        LoopContext{
            .break_label_ = end,
            .continue_label_ = counting ? count_continue : inc,
            .break_depth_ = base,
            .continue_depth_ = base,
            .block_depth_ = blocks_.size(),
        }
    );
    // 循环体恒按 want_value=true 编译，每轮的值都要被收尾逻辑读一次
    gen(node.body_, true);
    gen_loop_collect(node.collect_, has_slot, 1, count_continue, pos);
    loops_.pop();

    builder().place_label(inc);
    if (node.inc_) gen(node.inc_, false);
    builder().emit_jump(OpCode::JUMP, loop, pos);

    builder().place_label(end);
    if (has_slot && !want) builder().emit(OpCode::POP_TOP, pos);
}

void CodeGen::visit(const AstNodeForIter &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};
    const bool counting{node.collect_.container_ == CollectMark::Container::None};

    const bool has_slot{gen_loop_slot(node.collect_, want, pos)};
    const std::uint32_t after_slot{builder().stack_depth()};

    gen(node.iterable_, true);
    builder().emit(OpCode::GET_ITER, node.iterable_->pos_);

    // 栈序只能是"结果槽在下、迭代器在上"：FOR_ITER 要求迭代器在栈顶，而结果槽要活过整个循环
    const std::uint32_t base{builder().stack_depth()};

    const CodeBuilder::Label loop{builder().new_label()};
    const CodeBuilder::Label end{builder().new_label()};
    const CodeBuilder::Label count_continue{builder().new_label()};

    builder().place_label(loop);
    builder().emit_jump(OpCode::FOR_ITER, end, pos);

    if (node.target_)
        gen_store_one(*node.target_);
    else
        builder().emit(OpCode::POP_TOP, pos);

    loops_.push(
        LoopContext{
            .break_label_ = end,
            .continue_label_ = counting ? count_continue : loop,
            .break_depth_ = after_slot,
            .continue_depth_ = base,
            .block_depth_ = blocks_.size(),
        }
    );
    gen(node.body_, true);
    // 每轮的值压在迭代器上面，收尾指令要跨过它才够得着结果槽
    gen_loop_collect(node.collect_, has_slot, 2, count_continue, pos);
    loops_.pop();

    builder().emit_jump(OpCode::JUMP, loop, pos);

    builder().place_label(end);
    if (has_slot && !want) builder().emit(OpCode::POP_TOP, pos);
}

void CodeGen::visit(const AstNodeBreak &node) {
    if (loops_.empty()) error_internal("break outside a loop", node.pos_);

    const LoopContext loop{loops_.top()};
    gen_unwind(loop.block_depth_, loop.break_depth_, node.pos_);
    builder().emit_jump(OpCode::JUMP, loop.break_label_, node.pos_);
}

void CodeGen::visit(const AstNodeContinue &node) {
    if (loops_.empty()) error_internal("continue outside a loop", node.pos_);

    const LoopContext loop{loops_.top()};
    gen_unwind(loop.block_depth_, loop.continue_depth_, node.pos_);
    builder().emit_jump(OpCode::JUMP, loop.continue_label_, node.pos_);
}

void CodeGen::visit(const AstNodeReturn &node) {
    const Position pos{node.pos_};

    if (node.value_)
        gen(node.value_, true);
    else
        gen_none(pos);

    gen_unwind_with_value(pos);
    builder().emit(OpCode::RETURN_VALUE, pos);
}

void CodeGen::visit(const AstNodeTry &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};
    const std::uint32_t entry{builder().stack_depth()};

    CodeBuilder::Label finally_body{};
    if (node.finally_expr_) {
        finally_body = builder().new_label();
        builder().emit_jump(OpCode::SETUP_FINALLY, finally_body, pos);
        blocks_.push_back(
            BlockContext{.is_finally_ = true, .finally_label_ = finally_body, .entry_depth_ = entry}
        );
    }

    if (node.except_clauses_.empty()) {
        gen(node.try_expr_, want);
    } else {
        const CodeBuilder::Label handlers{builder().new_label()};
        const CodeBuilder::Label after{builder().new_label()};

        builder().emit_jump(OpCode::SETUP_EXCEPT, handlers, pos);
        blocks_.push_back(BlockContext{.entry_depth_ = entry});
        gen(node.try_expr_, want);
        blocks_.pop_back();
        builder().emit(OpCode::POP_BLOCK, pos);
        builder().emit_jump(OpCode::JUMP, after, pos);

        // 处理器入口栈上只有那个异常对象，CHECK_EXC_MATCH 会把它留下，不用额外复制
        builder().place_label(handlers);
        for (const AstNodeTry::AstNodeExceptAndExpr &clause : node.except_clauses_) {
            const CodeBuilder::Label next{builder().new_label()};
            const CodeBuilder::Label matched{builder().new_label()};

            for (std::size_t i{0}; i < clause.exceptions_.size(); ++i) {
                gen(clause.exceptions_[i], true);
                builder().emit(OpCode::CHECK_EXC_MATCH, pos);

                // 一条子句列了多个异常类型时，任一匹配就算中
                if (i + 1 < clause.exceptions_.size()) {
                    const CodeBuilder::Label try_next{builder().new_label()};

                    builder().emit_jump(OpCode::JUMP_IF_FALSE, try_next, pos);
                    builder().emit_jump(OpCode::JUMP, matched, pos);
                    builder().place_label(try_next);
                } else {
                    builder().emit_jump(OpCode::JUMP_IF_FALSE, next, pos);
                }
            }

            builder().place_label(matched);
            if (clause.target_)
                gen_store_one(*clause.target_);
            else
                builder().emit(OpCode::POP_TOP, pos);

            gen(clause.body_, want);
            builder().emit_jump(OpCode::JUMP, after, pos);
            builder().place_label(next);
        }

        builder().emit(OpCode::RERAISE, pos);
        builder().place_label(after);
    }

    if (!node.finally_expr_) return;

    blocks_.pop_back();
    builder().emit(OpCode::POP_BLOCK, pos);

    // 进 finally 体时栈顶恒是"这条路径要带出去的那个值"。正常路径带的是 try 的值，
    // 没人要值时就压个 None 占位，好让异常路径和这里的栈深对得上
    if (!want) gen_none(pos);
    builder().emit_jump(OpCode::CALL_FINALLY, finally_body, pos);
    if (!want) builder().emit(OpCode::POP_TOP, pos);

    const CodeBuilder::Label after_all{builder().new_label()};
    builder().emit_jump(OpCode::JUMP, after_all, pos);

    // finally 体只编译一份，正常路径和异常路径共用，末尾统一由 END_FINALLY 分派
    builder().place_label(finally_body);
    gen(node.finally_expr_, false);
    builder().emit(OpCode::END_FINALLY, pos);

    builder().place_label(after_all);
}

void CodeGen::visit(const AstNodeRaise &node) {
    gen(node.value_, true);
    builder().emit(OpCode::RAISE, node.pos_);
}

void CodeGen::visit(const AstNodeDecorator &node) {
    const bool want{want_value_};

    // @d expr 就是 d(expr)
    gen(node.decorator_, true);
    gen(node.target_, true);
    builder().emit(OpCode::CALL, 1, node.pos_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeEval &node) {
    const bool want{want_value_};

    // eval 不能像 import 那样包成一个内部函数：它要拿到调用帧的名字环境
    gen_call_args_packed(node.args_, node.args_.pos_paren_);
    builder().emit(OpCode::EVAL, node.pos_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeFunc &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (!node.body_) error_internal("function without a body", pos);

    // 先把函数体编译成一份嵌套 Code
    begin_code();
    declare_captures(node.captures_);
    builder().set_params(build_param_shape(node));
    gen_program(*node.body_);
    Ref body{end_code()};

    const std::uint32_t slot{builder().add_nested_code(std::move(body))};

    gen_decorators_eval(node.decorators_);
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
    builder().emit(OpCode::MAKE_TUPLE, param_items, pos);

    gen_optional(node.return_type_, pos);
    gen_optional(node.doc_, pos);
    if (node.name_)
        gen_load_const(make_ref<Str>(*node.name_), pos);
    else
        gen_missing(pos);

    builder().emit(OpCode::MAKE_FUNC, slot, pos);
    gen_decorators_apply(node.decorators_, pos);

    // 命名函数绑定一次；匿名的没有名字可绑
    if (node.name_) {
        if (want) builder().emit(OpCode::COPY, 1, pos);
        builder().emit(OpCode::STORE_NAME, builder().intern_name(*node.name_), pos);
    } else if (!want) {
        builder().emit(OpCode::POP_TOP, pos);
    }
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
    builder().emit(OpCode::IMPORT, builder().intern_name(dotted), pos);
    if (want) builder().emit(OpCode::COPY, 1, pos);
    builder().emit(OpCode::STORE_NAME, builder().intern_name(node.segments_.front()), pos);
}

void CodeGen::visit(const AstNodeImportCall &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    // 实参形状跟普通调用一致，所以加载算法包成一个内部函数对象接一次普通调用即可
    builder().emit(
        OpCode::LOAD_INTERNAL, static_cast<std::uint32_t>(InternalObject::ImportFunc), pos
    );
    gen_call(node.args_, node.args_.pos_paren_);
    if (!want) builder().emit(OpCode::POP_TOP, pos);
}

void CodeGen::visit(const AstNodeLiteralNone &node) {
    if (want_value_) gen_load_const(literal_object(node), node.pos_);
}

void CodeGen::visit(const AstNodeLiteralBool &node) {
    if (want_value_) gen_load_const(literal_object(node), node.pos_);
}

void CodeGen::visit(const AstNodeLiteralGL &node) {
    // 压的是 _L / _G 那个字典本身，不是视图也不是快照
    if (want_value_)
        builder().emit(
            OpCode::LOAD_GL, node.value_ == AstNodeLiteralGL::GLType::G ? 1u : 0u, node.pos_
        );
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

void CodeGen::visit(const AstNodeLiteralTuple &node) {
    const bool want{want_value_};

    // 元组不可变，元素全是常量时整个进常量表，一条 LOAD_CONST 就够
    if (const ObjectRef value{literal_object(node)}) {
        if (want) gen_load_const(value, node.pos_);

        return;
    }

    gen_sequence(node.items_, true, node.pos_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeLiteralList &node) {
    const bool want{want_value_};

    // list 可变，每次求值都得是新对象，元素全是常量也不例外
    gen_sequence(node.items_, false, node.pos_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeLiteralDict &node) {
    const bool want{want_value_};

    gen_dict(node.items_, node.pos_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeLiteralEllipsis &node) {
    if (want_value_) gen_load_const(literal_object(node), node.pos_);
}

void CodeGen::visit(const AstNodeProgram &node) {
    // Program 只出现在文件 / 函数体 / 类体三处，各自走 gen_program，不会当子表达式分派到这里
    error_internal("a program cannot appear as a sub-expression", node.pos_);
}

void CodeGen::visit(const AstNodeCompound &node) { gen_exprs(node.exprs_, want_value_, node.pos_); }

void CodeGen::visit(const AstNodeStar &node) {
    error_internal("* is only allowed in containers, call arguments and destructuring", node.pos_);
}

void CodeGen::visit(const AstNodeDoubleStar &node) {
    error_internal("** is only allowed in dict literals and call arguments", node.pos_);
}

void CodeGen::visit(const AstNodeOpUnary &node) {
    const bool want{want_value_};

    gen(node.operand_, true);
    builder().emit(OpCode::UNARY_OP, to_unary_op(node.op_), node.pos_op_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeOpBinary &node) {
    const bool want{want_value_};
    const Position pos{node.pos_op_};

    const bool is_and{node.op_ == AstNodeOpBinary::OpType::And};
    const bool is_or{node.op_ == AstNodeOpBinary::OpType::Or};

    if (!is_and && !is_or) {
        gen(node.left_, true);
        gen(node.right_, true);
        builder().emit(OpCode::BINARY_OP, to_binary_op(node.op_), pos);
        if (!want) builder().emit(OpCode::POP_TOP, node.pos_);

        return;
    }

    // and / or 不落在 BINARY_OP 上，只用消费型的 JUMP_IF_FALSE 短路。
    // want 时先复制一份去做真值测试，原件留在栈上当短路的结果
    const CodeBuilder::Label end{builder().new_label()};

    gen(node.left_, true);
    if (want) builder().emit(OpCode::COPY, 1, pos);
    builder().emit(OpCode::TO_BOOL, pos);

    if (is_and) {
        builder().emit_jump(OpCode::JUMP_IF_FALSE, end, pos);
        if (want) builder().emit(OpCode::POP_TOP, pos);
        gen(node.right_, want);
    } else {
        // 没有 JUMP_IF_TRUE：把"假才跳"倒过来，再加一条无条件跳
        const CodeBuilder::Label rhs{builder().new_label()};

        builder().emit_jump(OpCode::JUMP_IF_FALSE, rhs, pos);
        builder().emit_jump(OpCode::JUMP, end, pos);
        builder().place_label(rhs);
        if (want) builder().emit(OpCode::POP_TOP, pos);
        gen(node.right_, want);
    }

    builder().place_label(end);
}

void CodeGen::visit(const AstNodeCompare &node) {
    const bool want{want_value_};

    if (node.ops_.empty() || node.operands_.size() != node.ops_.size() + 1 ||
        node.positions_op_.size() != node.ops_.size())
        error_internal("malformed comparison chain", node.pos_);

    std::vector<ChainStep> steps;
    steps.reserve(node.ops_.size());
    for (std::size_t i{0}; i < node.ops_.size(); ++i)
        steps.push_back(
            ChainStep{.op_ = to_binary_op(node.ops_[i]), .pos_ = node.positions_op_[i]}
        );

    gen_chain(node.operands_, steps, want);
}

void CodeGen::visit(const AstNodeIs &node) {
    const bool want{want_value_};

    if (node.positions_op_.empty() || node.positions_op_.size() + 1 != node.operands_.size())
        error_internal("malformed is chain", node.pos_);

    std::vector<ChainStep> steps;
    steps.reserve(node.positions_op_.size());
    for (const Position pos : node.positions_op_)
        steps.push_back(ChainStep{.op_ = BinaryOp::Is, .pos_ = pos});

    gen_chain(node.operands_, steps, want);
}

void CodeGen::visit(const AstNodeAssign &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    const std::uint32_t prefix{gen_target_prefix(*node.target_, false)};
    gen(node.value_, true);

    // want 时垫一份出来，让写入指令把原件弹光
    if (want) {
        builder().emit(OpCode::COPY, 1, pos);
        if (prefix > 0) builder().emit(OpCode::INSERT, prefix + 2, pos);
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
    builder().emit(OpCode::BINARY_OP, to_binary_op(node.op_), pos);

    if (want) {
        builder().emit(OpCode::COPY, 1, pos);
        if (prefix > 0) builder().emit(OpCode::INSERT, prefix + 2, pos);
    }

    gen_target_store(*node.target_);
}

void CodeGen::visit(const AstNodeCall &node) {
    const bool want{want_value_};

    gen(node.object_, true);
    gen_call(node.args_, node.args_.pos_paren_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeIndex &node) {
    const bool want{want_value_};
    const Position pos{node.pos_bracket_};

    gen(node.object_, true);

    // 下标里出现 *expr 时个数不再是编译期常数，先拼成一个 list
    if (has_expansion(node.args_)) {
        gen_sequence(node.args_, false, pos);
        builder().emit(OpCode::GET_INDEX_EX, pos);
    } else {
        for (const AstNodePtr &arg : node.args_) gen(arg, true);
        builder().emit(OpCode::GET_INDEX, static_cast<std::uint32_t>(node.args_.size()), pos);
    }

    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeAttr &node) {
    const bool want{want_value_};

    gen(node.object_, true);
    builder().emit(OpCode::GET_ATTR, builder().intern_name(node.attr_), node.pos_dot_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeIdentifier &node) {
    const bool want{want_value_};

    // 读不到就是 NameError，是指令固有行为，没人要值也照发
    builder().emit(OpCode::LOAD_NAME, builder().intern_name(node.identifier_), node.pos_);
    if (!want) builder().emit(OpCode::POP_TOP, node.pos_);
}

void CodeGen::visit(const AstNodeDel &node) {
    const bool want{want_value_};
    const Position pos{node.pos_};

    if (const auto *name{dynamic_cast<const AstNodeIdentifier *>(node.target_.get())}) {
        builder().emit(OpCode::DELETE_NAME, builder().intern_name(name->identifier_), pos);
    } else if (const auto *attr{dynamic_cast<const AstNodeAttr *>(node.target_.get())}) {
        gen(attr->object_, true);
        builder().emit(OpCode::DELETE_ATTR, builder().intern_name(attr->attr_), attr->pos_dot_);
    } else {
        error_internal("del target must be an identifier or an attribute access", pos);
    }

    if (want) gen_none(pos);
}

void CodeGen::visit(const AstNodeGlobal &node) {
    const bool want{want_value_};

    builder().emit(OpCode::DECLARE_GLOBAL, builder().intern_name(node.identifier_), node.pos_);
    if (want) gen_none(node.pos_);
}

Ref<Code> CodeGen::generate_program(const AstNodeProgram &root, std::string file_path) {
    if (!Runtime::ready()) throw InternalError{"CodeGen: runtime is not initialized"};

    CodeGen generator{std::move(file_path)};

    generator.begin_code();
    generator.gen_program(root);
    return generator.end_code();
}
