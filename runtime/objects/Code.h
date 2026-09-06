#pragma once

#include "../../utils/memory_utils.h"
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

        // 自报额外占的堆内存，供 mem::heap_bytes 递归到（两个 Ref 指向的是独立堆对象，不计）
        [[nodiscard]] std::size_t heap_bytes() const {
            return mem::heap_bytes(positional) + mem::heap_bytes(kw_only);
        }
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
        std::vector<ObjectRef> constants_table;
        std::vector<Ref<Code>> codes_table;
        std::vector<Ref<Str>> names_table;
        std::uint32_t max_stack{0};           // 栈深上限
        std::vector<bool> ref_captured;       // 名字表里是否是引用捕获，允许短于 names_table
        std::vector<Ref<Str>> value_captured; // 值捕获的名字
        std::optional<ParamShape> params;     // 函数体才有
        std::vector<LineEntry> lines_table;   // 行位置表，按 pc 升序
        std::string source_name;              // 文件名，或 eval 出来的那份写 "<eval>"
    };

  private:
    Parts parts_;

    SL_MAKE_REF_ONLY;
    explicit Code(Parts parts);

  public:
    // 一堆 getter
    [[nodiscard]] const std::vector<std::uint16_t> &bytecode() const { return parts_.bytecode; }
    [[nodiscard]] const std::vector<ObjectRef> &constants_table() const {
        return parts_.constants_table;
    }
    [[nodiscard]] const std::vector<Ref<Code>> &codes_table() const { return parts_.codes_table; }
    [[nodiscard]] const std::vector<Ref<Str>> &names_table() const { return parts_.names_table; }
    [[nodiscard]] std::uint32_t max_stack() const { return parts_.max_stack; }
    [[nodiscard]] bool is_ref_captured(std::size_t index) const;
    [[nodiscard]] const std::vector<Ref<Str>> &value_captured() const {
        return parts_.value_captured;
    }
    [[nodiscard]] const std::optional<ParamShape> &params() const { return parts_.params; }
    [[nodiscard]] const std::string &source_name() const { return parts_.source_name; }

    // pc 处指令对应的源码行列；表为空或 pc 落在第一项之前时返回空
    [[nodiscard]] std::optional<LineEntry> line_at(std::uint32_t pc) const;

    [[nodiscard]] std::size_t size_bytes() const override;

  private:
    void visit_own_refs(RefVisitor &visitor) override;
};
