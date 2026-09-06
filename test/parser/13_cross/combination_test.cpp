// 多种语法嵌套在一起时括号栈、换行、报错位置仍正确。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("跨分组组合——装饰器/类/函数/for/try/字典展开/is 揉在一起") {

    TEST_CASE(
        "装饰器+类+基类+装饰方法+捕获+**kwargs+for$+索引赋值+字典**展开+is+"
        "try多异常类型/finally 全部揉在一起，能正常解析且各自结构归属正确"
    ) {
        const std::u32string source =
            U"@register\n"
            U"class Handler(Base) {\n"
            U"    @cached\n"
            U"    func process[state](self, items, **opts) {\n"
            U"        result = {}\n"
            U"        for $ (items as item)\n"
            U"            try result[item.key] = {**opts, 'value': item.value, 'flag': item.kind "
            U"is 'ok'}\n"
            U"            except (ValueError, KeyError) raise Exception('propagate')\n"
            U"            finally item.seen = True\n"
            U"        return result\n"
            U"    }\n"
            U"}";
        CHECK_NOTHROW(parse_as_file(source));

        // 注意：这里必须用 = 而不是 const auto
        // root{parse_json(source)}——花括号初始化一个已经构造好的 json 对象会被 nlohmann 的
        // initializer_list<json> 构造函数当成"用这一个元素构造数组"
        const auto root = parse_json(source);
        CHECK(root["type"] == "Class");
        CHECK(root["decorators"] == nlohmann::json::array({ident("register")}));
        CHECK(root["bases"] == nlohmann::json::array({ident("Base")}));

        const auto &func_node{root["body"]["exprs"][0]};
        CHECK(func_node["type"] == "Func");
        CHECK(func_node["decorators"] == nlohmann::json::array({ident("cached")}));
        CHECK(
            func_node["captures"] ==
            nlohmann::json::array({nlohmann::json{
                {"capture_type", "Value"}, {"identifier", "state"}, {"value_expr", nullptr}
            }})
        );
        REQUIRE(func_node["params"]["positional"].size() == 2);
        CHECK(func_node["params"]["positional"][0]["identifier"] == "self");
        CHECK(func_node["params"]["var_kwargs_name"] == "opts");

        const auto &func_body{func_node["body"]["exprs"]};
        REQUIRE(func_body.size() == 3);

        const auto &for_node{func_body[1]};
        CHECK(for_node["type"] == "ForIter");
        CHECK(for_node["collect"] == "$");
        CHECK(for_node["target"] == ident("item"));
        CHECK(for_node["iterable"] == ident("items"));

        // for 的 body 没套 {}，直接就是 try 节点本身（不是 Compound 包一层）
        const auto &try_node{for_node["body"]};
        CHECK(try_node["type"] == "Try");
        CHECK(try_node["try_expr"]["type"] == "Assign");
        CHECK(try_node["try_expr"]["target"]["type"] == "Index");

        // 字典字面量里 ** 展开项和 is 表达式都被正确解析进对应的 key/val
        const auto &dict_val{try_node["try_expr"]["value"]};
        CHECK(dict_val["type"] == "LiteralDict");
        REQUIRE(dict_val["items"].size() == 3);
        CHECK(dict_val["items"][0]["key"]["type"] == "DoubleStar");
        CHECK(dict_val["items"][2]["value"]["type"] == "Is");

        REQUIRE(try_node["except_clauses"].size() == 1);
        CHECK(
            try_node["except_clauses"][0]["exceptions"] ==
            nlohmann::json::array({ident("ValueError"), ident("KeyError")})
        );
        CHECK(try_node["except_clauses"][0]["body"]["type"] == "Raise");
        CHECK(try_node["finally_expr"]["type"] == "Assign");

        CHECK(func_body[2]["type"] == "Return");
    }
}

TEST_SUITE("跨分组组合——{}/()/[] 混着嵌套时括号栈 brackets_ 的一致性") {

    TEST_CASE(
        "字典 value 换行不应合并这条规则，嵌套再深也不能失效（比 compound_dict_test.cpp 原始"
        "回归用例多包一层 list 和 call）"
    ) {
        // f( [ { 'a' : 1 \n + 2 } ] )：进入 {} 时 brackets_ 栈顶被压成 Brace，不管外层是
        // f( 还是 [ 嵌了几层，只要栈顶是 Brace，skip_paren_newline 就不会跳过换行——
        // 'a' 对应的 value 在换行处都应该老老实实结束，不能被外层的括号嵌套带偏而把 "+ 2"
        // 接续进来
        CHECK_THROWS_AS(parse_as_file(U"f([{'a': 1\n+ 2}])"), SyntaxError);
    }

    TEST_CASE(
        "字典结束后 brackets_ 栈顶正确恢复到外层，同一个调用里字典后面的实参依然支持跨行合并"
        "（parse_brace 必须在 finish_dict 返回之后才 expect_close(Brace) 弹栈，不能提前）"
    ) {
        const std::u32string source{U"f({'a': 1}, x\n+ y)"};
        CHECK_NOTHROW(parse_as_file(source));
        CHECK(
            parse_json(source) ==
            nlohmann::json{
                {"type", "Call"},
                {"object", ident("f")},
                {"positional_args",
                 nlohmann::json::array(
                     {{{"type", "LiteralDict"},
                       {"items",
                        nlohmann::json::array(
                            {{{"key", {{"type", "LiteralStr"}, {"value", "a"}}},
                              {"value", int_lit("1")}}}
                        )}},
                      {{"type", "OpBinary"},
                       {"op", "+"},
                       {"left", ident("x")},
                       {"right", ident("y")}}}
                 )},
                {"keyword_args", nlohmann::json::array()}
            }
        );
    }

    TEST_CASE(
        "for 头部某一槽本身是字典字面量（内部会各自独立压栈/弹栈 Bracket::Brace），"
        "不影响 parse_for 对槽间换行分隔符的判定"
    ) {
        // init 槽里的字典进入/退出时各自把 Brace 压栈/弹栈，brackets_ 栈底的 ForHeader
        // 全程不受影响，不应干扰槽间换行分隔。
        const std::u32string source = U"for (state = {'count': 0}\n"
                                      U"     state.count < 10\n"
                                      U"     state.count += 1) body";
        CHECK_NOTHROW(parse_as_file(source));

        const auto root = parse_json(source);
        CHECK(root["type"] == "ForCond");
        CHECK(root["init"]["type"] == "Assign");
        CHECK(root["init"]["value"]["type"] == "LiteralDict");
        CHECK(root["cond"]["type"] == "Compare");
        CHECK(root["inc"]["type"] == "CompoundAssign");
    }
}

TEST_SUITE("跨分组组合——深层嵌套结构里报错位置依然精确") {

    TEST_CASE(
        "list 套 dict 套圆括号，最深处漏写右操作数，报错行列精确指向那一行那一个 token，"
        "不会被外层任何一层结构带偏"
    ) {
        // row1: func f() {
        // row2: return [
        // row3: 1,
        // row4: {'a': (2 +
        // row5: )},
        // row6: ]
        // row7: }
        // 第 5 行开头的 ')' 就是"缺右操作数"真正暴露出来的地方（'+' 后面直接见到了 ')'）
        const std::u32string source = U"func f() {\n"
                                      U"return [\n"
                                      U"1,\n"
                                      U"{'a': (2 +\n"
                                      U")},\n"
                                      U"]\n"
                                      U"}";
        try {
            parse_as_file(source);
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("unexpected token ')'") != std::string::npos);
            CHECK(msg.find("5:1:") != std::string::npos);
        }
    }
}

TEST_SUITE("跨分组组合——表达式位置的通用性（默认值/实参/字典 key 都能放任意复杂表达式）") {

    TEST_CASE("形参默认值本身是一个匿名函数（func 是普通表达式，能出现在任何表达式能出现的位置）") {
        const std::u32string source{U"func f(x, cb = func(y) { return y + 1 }) { return cb(x) }"};
        CHECK_NOTHROW(parse_as_file(source));

        const auto j = parse_json(source);
        const auto &default_value{j["params"]["positional"][1]["default_value"]};
        CHECK(default_value["type"] == "Func");
        CHECK(default_value["name"] == nullptr);
        REQUIRE(default_value["params"]["positional"].size() == 1);
        CHECK(default_value["params"]["positional"][0]["identifier"] == "y");
    }

    TEST_CASE("if-else 作为普通表达式直接用作调用实参（一切皆表达式，不是只能当语句用）") {
        CHECK(
            parse_json(U"f(if (a) 1 else 2)") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", ident("f")},
                {"positional_args",
                 nlohmann::json::array(
                     {{{"type", "If"},
                       {"clauses",
                        nlohmann::json::array({{{"cond", ident("a")}, {"body", int_lit("1")}}})},
                       {"else_expr", int_lit("2")}}}
                 )},
                {"keyword_args", nlohmann::json::array()}
            }
        );
    }

    TEST_CASE("链式比较和 is 可以混在字典的 key 位置（key 就是普通的 parse_expr，没有特殊限制）") {
        const auto j = parse_json(U"{a < b is c: 1}");
        const auto &key{j["items"][0]["key"]};
        CHECK(key["type"] == "Is");
        CHECK(key["operands"][0]["type"] == "Compare");
    }
}
