// SL.md 的运算符一节——`in`（成员测试，优先级 55）。
// 它夹在比较组（60）和 `is`（50）之间，两边都不混链；自己也不像它们那样收成链节点，
// 连写就是普通的左结合。
// 它同时是迭代 for 的判别依据（`for (target in iterable)`），那部分在
// 08_control_flow/for_test.cpp。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}

nlohmann::json in_op(const nlohmann::json &left, const nlohmann::json &right) {
    return nlohmann::json{{"type", "OpBinary"}, {"op", "in"}, {"left", left}, {"right", right}};
}

nlohmann::json binary(const char *op, const nlohmann::json &l, const nlohmann::json &r) {
    return nlohmann::json{{"type", "OpBinary"}, {"op", op}, {"left", l}, {"right", r}};
}

nlohmann::json compare_eq(const nlohmann::json &l, const nlohmann::json &r) {
    return nlohmann::json{
        {"type", "Compare"},
        {"operands", nlohmann::json::array({l, r})},
        {"ops", nlohmann::json::array({"=="})}
    };
}
} // namespace

TEST_SUITE("in 运算符") {

    TEST_CASE("基本形态：一个普通二元节点") {
        CHECK(parse_json(U"x in items") == in_op(ident("x"), ident("items")));
    }

    TEST_CASE("`in` 是关键字不是符号，两侧的空白省不掉") {
        // 省掉就跟相邻标识符粘成一个标识符，不再是三个 token
        CHECK(parse_json(U"ain") == ident("ain"));
        CHECK(parse_json(U"inb") == ident("inb"));
        // 于是 `ain b` 是两个挨着的标识符、`a inb` 也是，都缺分隔符
        CHECK_THROWS_AS(parse_as_file(U"ain b"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"a inb"), SyntaxError);
    }

    TEST_CASE("换行：in 之后可以（右操作数会跨行找），in 之前不行（左边已经完整）") {
        CHECK(parse_json(U"a in\nb") == in_op(ident("a"), ident("b")));
        // 跟 SL.md 表达式分隔符一节的例子同理：第一行本身完整，就此断开，下一行以 in 开头起不了头
        CHECK_THROWS_AS(parse_as_file(U"a\nin b"), SyntaxError);
    }
}

TEST_SUITE("in 不主动支持链式，但连写也不报错") {

    // 比较组和 is 各自会把连写收成一个链节点（`a < b <= c`、`a is b is c`），in 不参与这套：
    // 它就是个普通的左结合二元运算符，连写就按左结合叠上去，语法层不拦
    TEST_CASE("`a in b in c` 就是 `(a in b) in c`") {
        CHECK(parse_json(U"a in b in c") == in_op(in_op(ident("a"), ident("b")), ident("c")));
        CHECK(parse_json(U"a in b in c") == parse_json(U"(a in b) in c"));
        // 结果通常在运行期抛 TypeError，但那是运行期的事，跟解析无关
    }

    TEST_CASE("加括号能改结合方向") {
        CHECK(parse_json(U"a in (b in c)") == in_op(ident("a"), in_op(ident("b"), ident("c"))));
    }
}

TEST_SUITE("in 的优先级（55：紧于 is、松于比较组）") {

    TEST_CASE("比算术、范围松：算术那边先结合") {
        CHECK(parse_json(U"a + b in c") == in_op(binary("+", ident("a"), ident("b")), ident("c")));
        CHECK(parse_json(U"a in b + c") == in_op(ident("a"), binary("+", ident("b"), ident("c"))));
        CHECK(
            parse_json(U"a in b .. c") == in_op(ident("a"), binary("..", ident("b"), ident("c")))
        );
    }

    TEST_CASE("比比较组松：比较先结合，两者不混链") {
        CHECK(parse_json(U"a in b == c") == in_op(ident("a"), compare_eq(ident("b"), ident("c"))));
        CHECK(parse_json(U"a == b in c") == in_op(compare_eq(ident("a"), ident("b")), ident("c")));
    }

    TEST_CASE("比 is 紧：in 先结合，两者不混链") {
        CHECK(
            parse_json(U"a in b is c") ==
            nlohmann::json{
                {"type", "Is"},
                {"operands", nlohmann::json::array({in_op(ident("a"), ident("b")), ident("c")})}
            }
        );
    }

    TEST_CASE("比 not / and / or / 赋值紧") {
        CHECK(
            parse_json(U"not a in b") ==
            nlohmann::json{
                {"type", "OpUnary"}, {"op", "not"}, {"operand", in_op(ident("a"), ident("b"))}
            }
        );
        CHECK(
            parse_json(U"a in b and c in d") ==
            binary("and", in_op(ident("a"), ident("b")), in_op(ident("c"), ident("d")))
        );
        CHECK(
            parse_json(U"r = a in b") ==
            nlohmann::json{
                {"type", "Assign"}, {"target", ident("r")}, {"value", in_op(ident("a"), ident("b"))}
            }
        );
    }
}
