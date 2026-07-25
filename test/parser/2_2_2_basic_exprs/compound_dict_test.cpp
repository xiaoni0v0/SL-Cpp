// SL.md 2.2.2 基本表达式——`{}` 的判别规则：
//   func/class 之后的 {} 为函数体/类体；否则第一个表达式本身是 ** 展开项（分组括号不影响判定，
//   见 SL.md 3.6）或其后紧跟 ':' 为字典字面量；都不满足为复合表达式。
// 这里只测判别规则本身和复合表达式的形状；字典各类项的具体语义（key 是否任意表达式等）已在
// 2_1_4_literals/literals_test.cpp 测过。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("2.2.2 {} 判别规则") {

    TEST_CASE("空 {} → 空的复合表达式，不是空字典（空字典专门写法是 dict()）") {
        CHECK(
            parse_json(U"{}") ==
            nlohmann::json{{"type", "Compound"}, {"exprs", nlohmann::json::array()}}
        );
    }

    TEST_CASE("第一项以 ** 开头 → 字典字面量") {
        CHECK(
            parse_json(U"{**d}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array(
                              {{{"key", {{"type", "DoubleStar"}, {"operand", ident("d")}}},
                                {"val", nullptr}}}
                          )}
            }
        );
    }

    TEST_CASE("第一个表达式后紧跟 ':' → 字典字面量") {
        CHECK(
            parse_json(U"{k: v}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array({{{"key", ident("k")}, {"val", ident("v")}}})}
            }
        );
    }

    TEST_CASE("紧跟 ':' 之前允许换行，仍然判成字典") {
        CHECK(
            parse_json(U"{k\n: v}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array({{{"key", ident("k")}, {"val", ident("v")}}})}
            }
        );
    }

    TEST_CASE("都不满足 → 复合表达式：单条表达式") {
        CHECK(
            parse_json(U"{x}") ==
            nlohmann::json{{"type", "Compound"}, {"exprs", nlohmann::json::array({ident("x")})}}
        );
    }

    TEST_CASE("复合表达式：分号分隔多条") {
        CHECK(
            parse_json(U"{a; b; c}") ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs", nlohmann::json::array({ident("a"), ident("b"), ident("c")})}
            }
        );
    }

    TEST_CASE("复合表达式：换行分隔多条，不需要分号") {
        CHECK(
            parse_json(U"{a\nb\nc}") ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs", nlohmann::json::array({ident("a"), ident("b"), ident("c")})}
            }
        );
    }

    TEST_CASE("复合表达式可以嵌套：{{}} 是外层复合表达式，唯一一条子表达式是内层的空复合表达式") {
        CHECK(
            parse_json(U"{{}}") ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs", nlohmann::json::array(
                              {{{"type", "Compound"}, {"exprs", nlohmann::json::array()}}}
                          )}
            }
        );
    }

    TEST_CASE("func/class 之后的 {} 是函数体/类体（Program），不走字典/复合表达式的判别") {
        const AstNodePtr node{parse_single(U"func f() {}")};
        const auto *func_node{dynamic_cast<AstNodeFunc *>(node.get())};
        REQUIRE(func_node != nullptr);
        CHECK(func_node->body_->exprs_.empty());

        const AstNodePtr class_node{parse_single(U"class C {}")};
        const auto *cls{dynamic_cast<AstNodeClass *>(class_node.get())};
        REQUIRE(cls != nullptr);
        CHECK(cls->body_->exprs_.empty());
    }

    TEST_CASE("{a, b} 两不像：既不是字典也不是合法复合表达式，必须报错（SL 没有集合字面量语法）") {
        CHECK_THROWS_AS(parse_program(U"{a, b}"), SyntaxError);
    }

    TEST_CASE("字典项之间只能用逗号分隔：';' 或单独的换行都不行") {
        CHECK_THROWS_AS(parse_program(U"{k: v; k2: v2}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{k: v\nk2: v2}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{**d; x}"), SyntaxError);
    }

    TEST_CASE("残缺的键值对报错") {
        CHECK_THROWS_AS(parse_program(U"{k:}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{: v}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{k: v,,}"), SyntaxError);
    }
}

TEST_SUITE("2.2.2 字典展开项：按表达式本身是不是 ** 展开判定（不是靠有没有冒号反推）") {

    TEST_CASE("展开项可以出现在第一项之外的位置") {
        CHECK(
            parse_json(U"{k: v, **d2}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array(
                              {{{"key", ident("k")}, {"val", ident("v")}},
                               {{"key", {{"type", "DoubleStar"}, {"operand", ident("d2")}}},
                                {"val", nullptr}}}
                          )}
            }
        );
    }

    TEST_CASE("展开项可以有多个，穿插在普通键值对之间") {
        CHECK(
            parse_json(U"{**d1, k: v, **d2}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array(
                              {{{"key", {{"type", "DoubleStar"}, {"operand", ident("d1")}}},
                                {"val", nullptr}},
                               {{"key", ident("k")}, {"val", ident("v")}},
                               {{"key", {{"type", "DoubleStar"}, {"operand", ident("d2")}}},
                                {"val", nullptr}}}
                          )}
            }
        );
    }

    TEST_CASE("展开项后面也支持尾逗号") {
        CHECK(
            parse_json(U"{k: v, **d2,}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array(
                              {{{"key", ident("k")}, {"val", ident("v")}},
                               {{"key", {{"type", "DoubleStar"}, {"operand", ident("d2")}}},
                                {"val", nullptr}}}
                          )}
            }
        );
    }

    TEST_CASE("第二项及以后既不是 ** 展开也没有冒号，必须报错，不能被静默当成合法展开项") {
        CHECK_THROWS_AS(parse_program(U"{k: v, x}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{k: v, x, y: z}"), SyntaxError);
    }

    TEST_CASE("展开项外面套分组括号是透明的：{(**d)} 就是 {**d}（与 f((**d)) ≡ f(**d) 一致）") {
        CHECK(parse_json(U"{(**d)}") == parse_json(U"{**d}"));
        CHECK(parse_json(U"{((**d))}") == parse_json(U"{**d}"));
        CHECK(parse_json(U"{k: v, (**d2)}") == parse_json(U"{k: v, **d2}"));
        CHECK(parse_json(U"f((**d))") == parse_json(U"f(**d)"));
    }

    TEST_CASE("判定看的是整个表达式的根节点：** 展开只是子表达式时不算展开项") {
        // {**d + x} 的第一个表达式是二元加法（** 只作用到 d，加法把整个展开节点包了进去），
        // 根节点不是展开项、后面也没有冒号 → 复合表达式
        // （其中的展开节点位于非法位置，由语义层按 SL.md 3.6 拒绝，不是语法层的事）
        CHECK(parse_json(U"{**d + x}")["type"] == "Compound");
        // 已确定是字典后（有 k: v 项），后续项是 '**d + x' 这种根不是展开、又没冒号的表达式 → 报错
        CHECK_THROWS_AS(parse_program(U"{k: v, **d + x}"), SyntaxError);
    }

    TEST_CASE("加括号的展开项跟不加括号的一样不能带 value") {
        CHECK_THROWS_AS(parse_program(U"{(**d): v}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{**d: v}"), SyntaxError);
    }
}

TEST_SUITE("2.2.2 复合表达式内部也必须有合法分隔符") {

    TEST_CASE("判别用的 first 和后续表达式之间没有分隔符必须报错，跟顶层 a b 同一个错误") {
        CHECK_THROWS_AS(parse_program(U"{k v}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{a b; c}"), SyntaxError);
    }

    TEST_CASE("first 和后续表达式之间只要有合法分隔符（换行/分号）就没问题") {
        CHECK(
            parse_json(U"{a\nb}") ==
            nlohmann::json{
                {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}
            }
        );
        CHECK(
            parse_json(U"{a; b}") ==
            nlohmann::json{
                {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}
            }
        );
    }
}

TEST_SUITE("2.2.2 字典与复合表达式的其他边缘情况") {

    TEST_CASE("未闭合的 {} 抛异常") {
        CHECK_THROWS_AS(parse_program(U"{a; b"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{k: v"), SyntaxError);
    }

    TEST_CASE("字典值可以是复合表达式，复合表达式里也可以嵌字典") {
        CHECK(
            parse_json(U"{k: {a; b}}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array(
                              {{{"key", ident("k")},
                                {"val",
                                 {{"type", "Compound"},
                                  {"exprs", nlohmann::json::array({ident("a"), ident("b")})}}}}}
                          )}
            }
        );
        CHECK(
            parse_json(U"{ {k: v}; x }") ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "LiteralDict"},
                       {"items",
                        nlohmann::json::array({{{"key", ident("k")}, {"val", ident("v")}}})}},
                      ident("x")}
                 )}
            }
        );
    }

    TEST_CASE("前导 ';' 强制判为复合表达式，即使后面长得像字典的 'k: v' 也不能被判成字典") {
        // 一旦见到前导 ';'，字典这个可能性就被排除了；剩下的 "a : b" 不是合法的复合表达式项
        // （单条表达式后面不能直接跟 ':'），必须报错，不能被静默解析成 {a: b} 这样的字典
        CHECK_THROWS_AS(parse_program(U"{; a : b}"), SyntaxError);
    }

    TEST_CASE("前导 ';' 本身只是个空的起始分隔符，不影响后面正常的复合表达式") {
        CHECK(
            parse_json(U"{; a}") ==
            nlohmann::json{{"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a")})}}
        );
        CHECK(
            parse_json(U"{; a; b}") ==
            nlohmann::json{
                {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}
            }
        );
    }

    TEST_CASE("只有分隔符、没有任何表达式的 {} 仍是空复合表达式") {
        const auto empty_compound =
            nlohmann::json{{"type", "Compound"}, {"exprs", nlohmann::json::array()}};
        CHECK(parse_json(U"{;}") == empty_compound);
        CHECK(parse_json(U"{;;}") == empty_compound);
        CHECK(parse_json(U"{\n}") == empty_compound);
        CHECK(parse_json(U"{\n;\n}") == empty_compound);
    }

    TEST_CASE("尾随分隔符：{a;} / {a\\n} 都是单语句复合表达式") {
        const auto single =
            nlohmann::json{{"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a")})}};
        CHECK(parse_json(U"{a;}") == single);
        CHECK(parse_json(U"{a\n}") == single);
    }

    TEST_CASE("** 之后可以换行再接被展开的表达式") {
        CHECK(
            parse_json(U"{**\nd}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array(
                              {{{"key", {{"type", "DoubleStar"}, {"operand", ident("d")}}},
                                {"val", nullptr}}}
                          )}
            }
        );
    }
}

TEST_SUITE("2.2.2 {} 内部是独立的语句语境：块内换行不受外层括号续行规则影响") {

    // SL.md 2.2.1 的表达式分隔规则按"块"递归适用：括号内的换行视作续行（表达式不完整则合并），
    // 但一旦进入 {} 这个新的块，换行就重新充当语句分隔符——同一段 {…} 无论写在括号内还是括号外，
    // 解析结果必须一致

    TEST_CASE("调用实参里的复合表达式，内部换行照常分隔语句") {
        CHECK(
            parse_json(U"f({a\nb})") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", ident("f")},
                {"args", nlohmann::json::array(
                             {{{"type", "Compound"},
                               {"exprs", nlohmann::json::array({ident("a"), ident("b")})}}}
                         )},
                {"kwargs", nlohmann::json::array()}
            }
        );
    }

    TEST_CASE("列表元素里的复合表达式同理") {
        CHECK(
            parse_json(U"[{a\nb}]") ==
            nlohmann::json{
                {"type", "LiteralList"},
                {"items", nlohmann::json::array(
                              {{{"type", "Compound"},
                                {"exprs", nlohmann::json::array({ident("a"), ident("b")})}}}
                          )}
            }
        );
    }

    TEST_CASE("圆括号分组里的复合表达式同理") {
        CHECK(
            parse_json(U"({a\nb})") ==
            nlohmann::json{
                {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}
            }
        );
    }

    TEST_CASE("前导 ';' 分支同样在独立语境里解析") {
        CHECK(
            parse_json(U"f({; a\nb})")["args"][0] ==
            nlohmann::json{
                {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}
            }
        );
    }

    TEST_CASE("嵌套复合表达式：外层在调用实参里，内层还有自己的换行，逐层语境正确切换") {
        CHECK(
            parse_json(U"f({a\n{b\nc}})")["args"][0] ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs", nlohmann::json::array(
                              {ident("a"),
                               {{"type", "Compound"},
                                {"exprs", nlohmann::json::array({ident("b"), ident("c")})}}}
                          )}
            }
        );
    }

    TEST_CASE("调用实参里的多行字典照常工作（字典跨行靠自身对换行的显式容忍，与括号续行无关）") {
        CHECK(
            parse_json(U"f({\nk: v,\nk2: v2,\n})")["args"][0] ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array(
                              {{{"key", ident("k")}, {"val", ident("v")}},
                               {{"key", ident("k2")}, {"val", ident("v2")}}}
                          )}
            }
        );
    }

    TEST_CASE("调用实参里的字典：键与 ':' 之间的换行照常允许") {
        CHECK(
            parse_json(U"f({k\n: v})")["args"][0] ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array({{{"key", ident("k")}, {"val", ident("v")}}})}
            }
        );
    }

    TEST_CASE("字典的 value 续行判定同样不受外层括号影响：不会把换行后的运算符误接进 value") {
        // v 后面换行紧跟 '+'：块内语境若没有正确屏蔽外层括号的深度，会被误当成延续行
        // 合并成 "v + w"；正确行为是换行终止了这个 value，后面单独的 '+ w' 不构成
        // 合法的字典收尾，必须报错——跟没有外层调用包裹时的顶层 {k: v\n+ w} 完全一致
        CHECK_THROWS_AS(parse_program(U"{k: v\n+ w}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"f({k: v\n+ w})"), SyntaxError);
    }

    TEST_CASE("字典值是复合表达式、整体又在调用实参里：逐层语境正确切换") {
        CHECK(
            parse_json(U"f({k: {a\nb}})")["args"][0] ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", nlohmann::json::array(
                              {{{"key", ident("k")},
                                {"val",
                                 {{"type", "Compound"},
                                  {"exprs", nlohmann::json::array({ident("a"), ident("b")})}}}}}
                          )}
            }
        );
    }

    TEST_CASE("函数体/类体同样是独立语句语境（整个 func/class 写在调用实参里）") {
        CHECK(parse_json(U"f(func() {a\nb})")["args"][0]["body"]["exprs"].size() == 2);
        CHECK(parse_json(U"f(class C {a\nb})")["args"][0]["body"]["exprs"].size() == 2);
    }

    TEST_CASE("'}' 之后回到外层语境：括号内 '}' 后面的换行仍按括号续行规则合并") {
        CHECK(
            parse_json(U"f({a; b}\n.c)")["args"][0] ==
            nlohmann::json{
                {"type", "Attr"},
                {"object",
                 {{"type", "Compound"},
                  {"exprs", nlohmann::json::array({ident("a"), ident("b")})}}},
                {"attr", "c"}
            }
        );
    }

    TEST_CASE("顶层（无外层括号）'}' 后面的换行则正常终止语句，'.c' 不能开启新语句") {
        CHECK_THROWS_AS(parse_program(U"{a; b}\n.c"), SyntaxError);
    }

    TEST_CASE("块内语句该报的错照报：{} 写进括号里不会放松块内的分隔符要求") {
        // 括号内的 {a b}（两个语句之间既无换行也无 ';'）跟顶层一样必须报错
        CHECK_THROWS_AS(parse_program(U"f({a b})"), SyntaxError);
        // 括号内的 {; a : b} 跟顶层一样：前导 ';' 强制复合表达式，'a : b' 不合法
        CHECK_THROWS_AS(parse_program(U"f({; a : b})"), SyntaxError);
    }
}
