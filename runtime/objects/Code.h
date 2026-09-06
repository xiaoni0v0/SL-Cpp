#pragma once

#include "../Object.h"
#include "Str.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * 一份编译好的代码。
 */
class Code final : public Object {
  public:
    /**
     * 单个形参的形状。
     *
     * 默认值与类型注解本身不在 Code 里（它们挂在函数对象上），
     */
    struct OneParamShape {
        Ref<Str> name;
        bool has_default{false};
        bool has_annotation{false};
    };

    /**
     * 形参形状，只有函数体的 Code 有。
     */
    struct ParamShape {
        std::vector<OneParamShape> positional; // *args 之前
        std::vector<OneParamShape> kw_only;    // *args 之后、**kwargs 之前，只能按关键字传
        Ref<Str> var_args;                     // *identifier，空表示没有
        Ref<Str> var_kwargs;                   // **identifier，空表示没有
        bool has_return_annotation{false};
    };

    /**
     * 行位置表的一项
     */
    struct LineEntry {
        std::uint32_t pc{0};
        int row{0};
        int col{0};
    };

    /**
     * 建 Code 用的一整包字段。
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
