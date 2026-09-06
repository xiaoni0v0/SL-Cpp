#pragma once

#include "../Object.h"
#include "Str.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * 一份编译好的代码：一份文件、一个函数体、一个类体各一个。
 *
 * 是 SL 对象但 SL 层完全接触不到（SL.md 4.3.8）。做成对象是为了让引用计数与 GC 的追踪能穿过它——
 * 它强引用着常量表、嵌套 Code 表、各处名字，标记器只顺着 Object 走，不做成对象这条链就断了。
 *
 * 建好之后不再改变。字段清单与语义见 compiler/codegen/bytecode.md。
 */
class Code final : public Object {
  public:
    /**
     * 单个形参的形状。
     *
     * 默认值与类型注解本身不在 Code 里（它们在外层作用域求值、挂在函数对象上），
     * Code 只需要知道"有没有"——实参绑定要靠它决定缺参是报错还是取默认值，
     * 也要靠它把默认值/注解元组里的项对应回具体形参。
     */
    struct OneParam {
        Ref<Str> name;
        bool has_default{false};
        bool has_annotation{false};
    };

    /**
     * 形参形状，只有函数体的 Code 有。
     *
     * 照 SL.md 3.5 的四段切分成四个字段，不用打了 tag 的扁平表——
     * "第几个之后算 kw_only"这种状态机式的表达是漏边界情况的常客。
     */
    struct ParamShape {
        std::vector<OneParam> positional; // *args 之前
        std::vector<OneParam> kw_only;    // *args 之后、**kwargs 之前，只能按关键字传
        Ref<Str> var_args;                // *identifier，空表示没有
        Ref<Str> var_kwargs;              // **identifier，空表示没有
        bool has_return_annotation{false};
    };

    // 行位置表的一项：从 pc 起的指令都算在这个行列上
    struct LineEntry {
        std::uint32_t pc{0};
        int row{0};
        int col{0};
    };

    /**
     * 建 Code 用的一整包字段。
     *
     * Code 本身不可变，而 codegen 是一路往各张表里追加的；与其给 Code 开一串 setter 或者一个
     * 十来个参数的构造函数，不如让 codegen 填好这个聚合体再整个搬进去。
     */
    struct Parts {
        std::vector<std::uint16_t> bytecode;
        std::vector<ObjectRef> constants;
        std::vector<Ref<Code>> nested_codes;
        // 标识符、属性名、import 的点分名字。存 Str 对象而不是 u32string：
        // _L/_G 是真字典、键就是 str 对象，存好了每次名字读写就不用现造一个
        std::vector<Ref<Str>> names;

        // 操作数栈的深度上限，编译期算出，建帧时一次性分配
        std::uint32_t max_stack{0};

        // 引用捕获的名字，按 names 的下标标记（下标 i 为真表示 names[i] 是引用捕获的）。
        // 运行期每次按标识符读写都要查，所以做成位图而不是名字集合，查一次是 O(1)。
        // 长度应当与 names 一致；短了按"没标记"处理
        std::vector<bool> ref_captured;
        // 值捕获的名字，按声明顺序对应 MAKE_FUNC/MAKE_CLASS 弹的那份捕获值元组
        std::vector<Ref<Str>> value_captured;

        // 函数体才有
        std::optional<ParamShape> params;

        // 报错用，不是语言规范的一部分
        std::string source_name;      // 文件名，或 eval 出来的那份写 "<eval>"
        std::vector<LineEntry> lines; // 按 pc 升序
    };

  private:
    Parts parts_;

    SL_MAKE_REF_ONLY;
    // 不像别的对象类型那样收 Type*：SL 层拿不到 Code 实例，就不可能有 code 的子类
    explicit Code(Parts parts);

  public:
    [[nodiscard]] const std::vector<std::uint16_t> &bytecode() const { return parts_.bytecode; }
    [[nodiscard]] const std::vector<ObjectRef> &constants() const { return parts_.constants; }
    [[nodiscard]] const std::vector<Ref<Code>> &nested_codes() const { return parts_.nested_codes; }
    [[nodiscard]] const std::vector<Ref<Str>> &names() const { return parts_.names; }
    [[nodiscard]] std::uint32_t max_stack() const { return parts_.max_stack; }
    [[nodiscard]] const std::vector<Ref<Str>> &value_captured() const {
        return parts_.value_captured;
    }
    // 函数体才有；模块体/类体为空
    [[nodiscard]] const std::optional<ParamShape> &params() const { return parts_.params; }
    [[nodiscard]] const std::string &source_name() const { return parts_.source_name; }

    // names[index] 是不是引用捕获的。调用方保证 index < names().size()
    [[nodiscard]] bool is_ref_captured(std::size_t index) const;
    // pc 处指令对应的源码行列；表为空或 pc 落在第一项之前时返回空
    [[nodiscard]] std::optional<LineEntry> line_at(std::uint32_t pc) const;

    [[nodiscard]] std::size_t size_bytes() const override;

  private:
    void visit_own_refs(RefVisitor &visitor) override;
};
