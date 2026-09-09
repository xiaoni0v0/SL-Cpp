#include "test_utils.h"

#include <doctest/doctest.h>

namespace {

// n 个互不相同的整数字面量，逗号分隔
std::u32string distinct_ints(const int count) {
    std::u32string result;
    for (int i{0}; i < count; ++i) {
        if (i > 0) result += U',';
        for (const char digit : std::to_string(i)) result += static_cast<char32_t>(digit);
    }

    return result;
}

// 重复 count 遍 line
std::u32string repeat_line(const std::u32string &line, const int count) {
    std::u32string result;
    for (int i{0}; i < count; ++i) result += line;

    return result;
}

} // namespace

TEST_SUITE("codegen: 指令编码") {

    TEST_CASE("参数装得下一个字节时不补前缀") {
        const std::string dump{codegen_body(U"x = [" + distinct_ints(200) + U"]")};
        CHECK(dump.find("MAKE_LIST 200") != std::string::npos);
        CHECK(dump.find("LOAD_CONST 199") != std::string::npos);
    }

    TEST_CASE("参数超过一个字节时补 EXTENDED_ARG 前缀") {
        // 反汇编把前缀折进参数里，所以这里看到的是完整参数
        const std::string dump{codegen_body(U"x = [" + distinct_ints(300) + U"]")};
        CHECK(dump.find("LOAD_CONST 299") != std::string::npos);
        CHECK(dump.find("MAKE_LIST 300") != std::string::npos);
    }

    TEST_CASE("跳转目标是绝对的、以指令为单位") {
        CHECK(codegen_body(U"x = 1\ny = if (x) { x } else { x }") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 7
LOAD_NAME 0
JUMP 8
LOAD_NAME 0
STORE_NAME 1
)");
    }

    TEST_CASE("跳得远时目标也要补前缀，补完又把后面的推远，反复扫到不再变化") {
        const std::u32string source{
            U"x = 1\nif (x) {" + repeat_line(U"x\n", 300) + U"} else { x }"
        };
        const std::string dump{codegen_body(source)};

        // 近处那条不用前缀
        CHECK(dump.find("JUMP_IF_FALSE 6") != std::string::npos);

        // 跳过整个 then 分支的那条，目标越过了一个字节能表达的范围
        const std::size_t at{dump.rfind("\nJUMP ")};
        REQUIRE(at != std::string::npos);
        CHECK(std::stoi(dump.substr(at + 6)) > 255);
    }

    TEST_CASE("max_stack 是静态跟踪出来的峰值") {
        const std::string dump{codegen_dump(U"x = 1\ny = [x, x, x]")};
        // 三个元素同时在栈上
        CHECK(dump.find("max_stack=3") != std::string::npos);
    }

    TEST_CASE("行位置表只在行列变化时记一项") {
        const RuntimeFixture runtime;
        const Ref<Code> code{Compiler::compile(std::u32string{U"x = 1\ny = 2\n"}, "<test>")};

        const std::optional first{code->line_at(0)};
        REQUIRE(first.has_value());
        CHECK(first->row == 1);

        // 第二条语句的指令从 pc 2 起。末尾那条隐含的 return None 用的是整份 Program 的位置
        const std::optional second{code->line_at(2)};
        REQUIRE(second.has_value());
        CHECK(second->row == 2);
    }

    TEST_CASE("source_name 跟着传进来的文件路径走") {
        const RuntimeFixture runtime;
        const Ref<Code> code{Compiler::compile(std::u32string{U"1"}, "some/file.sl")};

        CHECK(code->source_name() == "some/file.sl");
    }

} // TEST_SUITE
