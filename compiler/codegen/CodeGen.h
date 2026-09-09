#pragma once

#include "../../runtime/objects/Code.h"
#include "../parser/ast_nodes/ast_nodes.h"
#include "CodeBuilder.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * AST -> Code。
 *
 * 每个编译节点的函数带一个 want_value。
 * 不变量是：编译完这个节点，操作数栈相对编译前恰好增加 want_value ? 1 : 0。
 *
 * 不会报 SyntaxError，只报 InternalError。
 *
 * 前提：Runtime 已经运行起来。
 */
class CodeGen final : public AstConstVisitor {
    const std::string file_path_;

    // builder 栈，栈顶是正在填的那份 Code。函数体、类体各压一层
    std::vector<std::unique_ptr<CodeBuilder>> builders_;

    // 成员通道：当前节点要不要产出值
    bool want_value_{true};

    // 一层循环。break/continue 是纯 PC 跳转，跳到哪、裁掉几层栈编译期就知道
    struct LoopContext {
        CodeBuilder::Label break_label_{};
        CodeBuilder::Label continue_label_{};
        std::uint32_t break_depth_{0};    // 跳到 break_label_ 处应有的栈深
        std::uint32_t continue_depth_{0}; // 跳到 continue_label_ 处应有的栈深
        std::size_t block_depth_{0};      // 进这层循环时外面套着几层块
    };

    // 一层运行期块（SETUP_EXCEPT / SETUP_FINALLY 压的那一项）。
    // 跳出去之前要由内到外逐层 POP_BLOCK，是 finally 的还要顺带走一遍 finally 体
    struct BlockContext {
        bool is_finally_{false};
        CodeBuilder::Label finally_label_{}; // 只有 is_finally_ 时有意义
        std::uint32_t entry_depth_{0};       // 进 try 时的栈深
    };

    // 比较链 / is 链的一段：一个运算符和它自己的位置
    struct ChainStep {
        BinaryOp op_{};
        Position pos_{};
    };

    std::vector<LoopContext> loops_;
    std::vector<BlockContext> blocks_;

    // 嵌套作用域期间存起来的外层上下文。函数体里的 break 跳不到外层循环
    std::vector<std::vector<LoopContext>> saved_loops_;
    std::vector<std::vector<BlockContext>> saved_blocks_;

    explicit CodeGen(std::string file_path);

    // 正在填的那份 Code
    [[nodiscard]] CodeBuilder &code() const;

    [[noreturn]] void error_internal(const std::string &msg, Position pos) const;

    // ---------------- 编译一个节点 ----------------

    void gen(const AstNode &node, bool want_value);
    void gen(const AstNodePtr &node, bool want_value);
    // 一整份 Program（文件 / 函数体 / 类体），连同末尾隐含的那条 return None
    void gen_program(const AstNodeProgram &program);
    // 复合表达式里的一串表达式，值是最后一条
    void gen_exprs(const std::vector<AstNodePtr> &exprs, bool want_value, Position pos);

    // ---------------- 小零件 ----------------

    void gen_load_const(const ObjectRef &value, Position pos);
    void gen_none(Position pos);
    // 弹到只剩 depth 项
    void gen_pop_to(std::uint32_t depth, Position pos);
    // 求值一个条件，假就跳走。JUMP_IF_FALSE 只认 bool，自己不做真值转换
    void gen_jump_if_false(const AstNode &cond, CodeBuilder::Label target);
    // 离开当前位置去别处：由内到外逐层退出运行期块，再把栈裁到 target_depth。
    // break / continue 用这条，它们不带值
    void gen_unwind(std::size_t to_block_depth, std::uint32_t target_depth, Position pos);
    // return 用这条：栈顶那个返回值要一路带着穿过每层块
    void gen_unwind_with_value(Position pos);

    // ---------------- 常量 ----------------

    // 字面量节点对应的 SL 对象。不是能进常量表的字面量就返回空
    [[nodiscard]] static ObjectRef literal_object(const AstNode &node);

    // ---------------- 容器 ----------------

    // items 里有没有 * / ** 展开项
    [[nodiscard]] static bool has_expansion(const std::vector<AstNodePtr> &items);
    // 逐项求值建 list / tuple。有展开项就先建再逐项并入，否则一条 MAKE_* 建好
    void gen_sequence(const std::vector<AstNodePtr> &items, bool as_tuple, Position pos);
    void gen_dict(const std::vector<std::pair<AstNodePtr, AstNodePtr>> &items, Position pos);

    // ---------------- 比较链 ----------------

    // 链不是新语义，编译成同一个运算符连续应用多次。比较链与 is 链共用
    void gen_chain(
        const std::vector<AstNodePtr> &operands, const std::vector<ChainStep> &steps, bool want
    );

    // ---------------- 赋值 ----------------

    // 把写入 target 需要的前置操作数压栈，返回压了几项。
    // under_value 表示待写入的值已经在栈顶，前置项要逐个垫到它下面
    std::uint32_t gen_target_prefix(const AstNode &target, bool under_value);
    // 前置操作数与值都就位，发写入指令。解构会递归展开到每个元素
    void gen_target_store(const AstNode &target);
    // 值在栈顶，把它写进 target。解构的每个元素走这条
    void gen_store_one(const AstNode &target);
    // 复合赋值读旧值：target 的 prefix 项前置操作数已在栈上，复制一份出来读
    void gen_target_load(const AstNode &target, std::uint32_t prefix);
    // 解构目标的元素清单；不是解构目标就返回空
    [[nodiscard]] static const std::vector<AstNodePtr> *destructuring_items(const AstNode &target);

    // ---------------- 调用 ----------------

    // 被调对象已在栈顶，压实参并发 CALL / CALL_KW / CALL_EX
    void gen_call(const CallArgs &args, Position pos);
    // 实参拼成 args(list) + kwargs(dict) 两项，EVAL 与 CALL_EX 用
    void gen_call_args_packed(const CallArgs &args, Position pos);

    // ---------------- 循环 ----------------

    // 结果槽：计数模式压 0，$ 压空 list，$$ 压空 dict。要不要压见返回值
    [[nodiscard]] bool gen_loop_slot(CollectMark collect, bool want_value, Position pos);
    // 每轮的收尾：把循环体的值折进结果槽。slot_depth 是结果槽在栈里的深度
    void gen_loop_collect(
        CollectMark collect, bool has_slot, std::uint32_t slot_depth,
        CodeBuilder::Label count_continue, Position pos
    );

    // ---------------- 嵌套作用域 ----------------

    // 进入一份新的 Code。循环/finally 上下文跟着换，函数体里的 break 跳不到外层循环
    void push_scope();
    [[nodiscard]] Ref<Code> pop_scope();
    // 捕获列表登记进新作用域：引用捕获标名字表位图，值捕获进 value_captured
    void declare_captures(const std::vector<OneCapture> &captures);
    // 外层这一侧：逐个求出捕获值并打成元组
    void gen_capture_values(const std::vector<OneCapture> &captures, Position pos);
    // 装饰器：建立之前从前到后求值，建立之后由近到远逐个调用
    void gen_decorators_before(const std::vector<AstNodePtr> &decorators);
    void gen_decorators_after(const std::vector<AstNodePtr> &decorators, Position pos);
    // 没写的注解/默认值/doc/名字用内部哨兵顶上，不能用 None（它是合法的默认值）
    void gen_missing(Position pos);
    // 可空的槽位：有就求值，没有就压哨兵
    void gen_optional(const AstNodePtr &node, Position pos);
    // 形参形状。默认值与注解本身不在 Code 里，这里只记有没有
    [[nodiscard]] static Code::ParamShape build_param_shape(const AstNodeFunc &node);

#define X(nt) void visit(const nt &node) override;
#include "../parser/ast_nodes/x_ast_nodes.inc"

#undef X

  public:
    /**
     * 编译一整份 Program。
     * @param root      分析完的 AST 根节点
     * @param file_path 文件路径，进 Code::source_name
     */
    [[nodiscard]] static Ref<Code> generate(const AstNodeProgram &root, std::string file_path);
};
