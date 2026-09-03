// int/bool 四则、幂、整除取模。decimal 和 `/` 不折。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 数值算术") {

    TEST_CASE("+ - * 纯 int") {
        CHECK(fold_json(U"1 + 2") == int_lit("3"));
        CHECK(fold_json(U"2 - 3") == int_lit("-1"));
        CHECK(fold_json(U"2 * 3") == int_lit("6"));
    }

    // bool 不继承 int；四则/比较会把 bool 折成 int。位运算、容器重复次数不接受 bool。
    TEST_CASE("bool 参与数值运算按 int 提升，结果类型是 int 不是 bool") {
        CHECK(fold_json(U"True + 1") == int_lit("2"));
        CHECK(fold_json(U"True + True") == int_lit("2"));
        CHECK(fold_json(U"+True") == int_lit("1"));
        CHECK(fold_json(U"-True") == int_lit("-1"));
    }

    // `/` 的结果恒为 decimal，哪怕两边都是 int、哪怕除得尽，也一律不折
    TEST_CASE("/ 恒产出 decimal，因此恒不折（即使两边都是 int、即使除得尽）") {
        CHECK(
            fold_json(U"7 / 2") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "/"}, {"left", int_lit("7")}, {"right", int_lit("2")}
            }
        );
        CHECK(
            fold_json(U"6 / 2") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "/"}, {"left", int_lit("6")}, {"right", int_lit("2")}
            }
        );
    }

    TEST_CASE("// 和 % 都是 int 时恒产出 int，向负无穷取整") {
        CHECK(fold_json(U"-7 // 2") == int_lit("-4"));
        CHECK(fold_json(U"-7 % 2") == int_lit("1"));
        CHECK(fold_json(U"7 // -2") == int_lit("-4"));
    }

    TEST_CASE("// 和 % 满足恒等式 x % y == x - (x // y) * y（正负操作数各种组合）") {
        CHECK(fold_json(U"(-7) % 2") == int_lit("1"));
        CHECK(fold_json(U"-7 - -7 // 2 * 2") == int_lit("1"));
        CHECK(fold_json(U"7 // 2") == int_lit("3"));
        CHECK(fold_json(U"7 % 2") == int_lit("1"));
        CHECK(fold_json(U"-7 // -2") == int_lit("3"));
        CHECK(fold_json(U"-7 % -2") == int_lit("-1"));
    }

    TEST_CASE("掺了 decimal 的 // 和 % 结果是 decimal，不折") {
        CHECK(
            fold_json(U"7.5 // 2") == nlohmann::json{
                                          {"type", "OpBinary"},
                                          {"op", "//"},
                                          {"left", decimal_lit("7.5")},
                                          {"right", int_lit("2")}
                                      }
        );
        CHECK(
            fold_json(U"-7.5 % 2") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "%"},
                {"left", {{"type", "OpUnary"}, {"op", "-"}, {"operand", decimal_lit("7.5")}}},
                {"right", int_lit("2")}
            }
        );
    }

    TEST_CASE("除以 0 一律不折，交给运行时报 MathError") {
        CHECK(
            fold_json(U"1 / 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "/"}, {"left", int_lit("1")}, {"right", int_lit("0")}
            }
        );
        CHECK(
            fold_json(U"1 // 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "//"}, {"left", int_lit("1")}, {"right", int_lit("0")}
            }
        );
        CHECK(
            fold_json(U"1 % 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "%"}, {"left", int_lit("1")}, {"right", int_lit("0")}
            }
        );
        CHECK(
            fold_json(U"1.0 / 0.0") == nlohmann::json{
                                           {"type", "OpBinary"},
                                           {"op", "/"},
                                           {"left", decimal_lit("1.0")},
                                           {"right", decimal_lit("0.0")}
                                       }
        );
    }

    TEST_CASE("** 都是 int 且指数非负，结果是 int") {
        CHECK(fold_json(U"2 ** 10") == int_lit("1024"));
        CHECK(fold_json(U"0 ** 0") == int_lit("1"));
        CHECK(fold_json(U"5 ** 0") == int_lit("1"));
    }

    TEST_CASE("** 指数为负，结果是 decimal，不折") {
        CHECK(
            // 注意右边是 int_lit("-1") 而不是 OpUnary：一元负号作用在 int 上是照折的，
            // 折完之后外层 ** 才发现指数为负、结果会是 decimal，于是停在这一步
            fold_json(U"2 ** -1") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "**"}, {"left", int_lit("2")}, {"right", int_lit("-1")}
            }
        );
    }

    // ** 优先级 150 且右结合，一元 +/-/~ 是 140：折叠是自底向上的，这两条不测就等于没验证
    // "先折哪一层"——折叠必须照着 Parser 已经搭好的树形状走，不能按自己的理解重新结合
    TEST_CASE("** 与一元负号的优先级、** 自身的右结合，折叠结果要跟这两条一致") {
        // -2 ** 2 解析成 -(2 ** 2)，先折 2 ** 2 = 4，外层一元负号再取负，结果 -4 不是 4
        CHECK(fold_json(U"-2 ** 2") == int_lit("-4"));
        // 2 ** 3 ** 2 右结合解析成 2 ** (3 ** 2)，先折内层 3 ** 2 = 9，外层 2 ** 9 = 512，
        // 不是按左结合理解出来的 (2 ** 3) ** 2 = 64
        CHECK(fold_json(U"2 ** 3 ** 2") == int_lit("512"));
    }

    TEST_CASE("+x/-x/~x 对字面量取值，~ 只对 bool/int 有意义") {
        CHECK(fold_json(U"-5") == int_lit("-5"));
        CHECK(fold_json(U"- -5") == int_lit("5"));
        CHECK(fold_json(U"~5") == int_lit("-6"));
        CHECK(fold_json(U"~0") == int_lit("-1"));
    }

    // decimal 的一元 +/- 也是算术运算，同样按上下文舍入（prec 小的时候 -1.234 会舍成 -1.2），
    // 不是恒等操作，所以跟 decimal 的二元算术一样不折
    TEST_CASE("+x/-x 作用在 decimal 上不折") {
        CHECK(
            fold_json(U"-1.5") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "-"}, {"operand", decimal_lit("1.5")}}
        );
        CHECK(
            fold_json(U"+1.5") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "+"}, {"operand", decimal_lit("1.5")}}
        );
    }

    TEST_CASE("范围运算不折") {
        CHECK(fold_json(U"1 .. 3")["type"] == "OpBinary");
        CHECK(fold_json(U"1 .. 3")["op"] == "..");
        CHECK(fold_json(U"1 .. 10 .. 2")["type"] == "OpBinary");
        CHECK(fold_json(U"(1 + 1) .. 3")["left"] == int_lit("2"));
    }

    TEST_CASE("后缀 ? / ! 不折") {
        CHECK(
            fold_json(U"1?") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "?"}, {"operand", int_lit("1")}}
        );
        CHECK(
            fold_json(U"1!") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "!"}, {"operand", int_lit("1")}}
        );
    }

    TEST_CASE("~ 对 decimal 不折，交给运行时报错") {
        CHECK(
            fold_json(U"~1.5") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "~"}, {"operand", decimal_lit("1.5")}}
        );
    }

    TEST_CASE("含变量/调用的子表达式不折，只递归折内部能折的部分") {
        CHECK(
            fold_json(U"x + 1") == nlohmann::json{
                                       {"type", "OpBinary"},
                                       {"op", "+"},
                                       {"left", {{"type", "Identifier"}, {"identifier", "x"}}},
                                       {"right", int_lit("1")}
                                   }
        );
        CHECK(
            fold_json(U"1 + 2 + x") == nlohmann::json{
                                           {"type", "OpBinary"},
                                           {"op", "+"},
                                           {"left", int_lit("3")},
                                           {"right", {{"type", "Identifier"}, {"identifier", "x"}}}
                                       }
        );
    }
}

// int 折叠只用 int64_t，不追求任意精度：溢出/装不下就不折，这是保守但正确。
// 这一组钉住 int64_t 的溢出边界，以及科学计数法展开的折/不折分界。
TEST_SUITE("StaticEvaler 数值算术——int64_t 边界") {

    TEST_CASE("+ - * ** 恰好越界就不折，交给运行时") {
        CHECK(
            fold_json(U"9223372036854775807 + 1") == nlohmann::json{
                                                         {"type", "OpBinary"},
                                                         {"op", "+"},
                                                         {"left", int_lit("9223372036854775807")},
                                                         {"right", int_lit("1")}
                                                     }
        );
        // 一元 - 对 9223372036854775807 照折（结果 -9223372036854775807 没超界），
        // 是外层 - 2 才溢出
        CHECK(
            fold_json(U"-9223372036854775807 - 2") == nlohmann::json{
                                                          {"type", "OpBinary"},
                                                          {"op", "-"},
                                                          {"left", int_lit("-9223372036854775807")},
                                                          {"right", int_lit("2")}
                                                      }
        );
        // 3037000500^2 == 9223372037000250000，比 INT64_MAX 大一点点
        CHECK(fold_json(U"3037000500 * 3037000500")["type"] == "OpBinary");
        // 2 ** 63 == 9223372036854775808，比 INT64_MAX 大 1
        CHECK(fold_json(U"2 ** 63")["type"] == "OpBinary");
    }

    TEST_CASE("紧贴 int64_t 边界但没越界的，照常折") {
        CHECK(fold_json(U"9223372036854775806 + 1") == int_lit("9223372036854775807"));
        CHECK(fold_json(U"-9223372036854775807 - 1") == int_lit("-9223372036854775808"));
        CHECK(fold_json(U"2 ** 62") == int_lit("4611686018427387904"));
    }

    // checked_pow 对 exponent==0、base∈{0,1,-1} 提前返回，否则 |base|>=2 时乘溢出就会停——
    // 去掉这几行特判，指数再大也会真的循环 exponent 次。跟空串重复同一个钉法（见
    // container_ops_test.cpp）：这里的回归失败形态是**测试跑不动**而不是断言红，取 int64_t
    // 上界的指数是最直白的钉法，不需要计时。
    TEST_CASE("** 的 0/1/-1 底数特判：指数再大也不会真的循环，不受指数上限影响") {
        CHECK(fold_json(U"1 ** 9223372036854775807") == int_lit("1"));
        CHECK(fold_json(U"0 ** 9223372036854775807") == int_lit("0"));
        CHECK(fold_json(U"(-1) ** 9223372036854775807") == int_lit("-1")); // 指数为奇数
        CHECK(fold_json(U"(-1) ** 9223372036854775806") == int_lit("1"));  // 指数为偶数
    }

    // "-9223372036854775808" 在源码里是一元负号作用在正数 9223372036854775808 上，而这个正数
    // 量级本身就超过 INT64_MAX（9223372036854775807），连里层的字面量都装不进 int64_t——
    // 这是二进制补码天生的不对称（负数比正数多能表示一个），保守放弃折叠是正确的，不是 bug
    TEST_CASE("源码里写 INT64_MIN 时，连里层的正数量级都装不下 int64_t，整个不折") {
        CHECK(
            fold_json(U"-9223372036854775808") ==
            nlohmann::json{
                {"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("9223372036854775808")}
            }
        );
    }

    // 按位取反不经过"先构造正数量级"这一步，能直接精确产出 INT64_MIN；用它间接构造出一个
    // raw_ 就是 "-9223372036854775808" 的字面量，才能测到 // 和 % 里 INT64_MIN / -1 那个溢出特例
    TEST_CASE("// 和 % 里 INT64_MIN / -1 会溢出，不折；% 因为恒无余数不受影响") {
        CHECK(fold_json(U"~9223372036854775807") == int_lit("-9223372036854775808"));
        CHECK(
            fold_json(U"~9223372036854775807 // -1") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "//"},
                {"left", int_lit("-9223372036854775808")},
                {"right", int_lit("-1")}
            }
        );
        // INT64_MIN % -1 数学上恒为 0，不涉及溢出，照折
        CHECK(fold_json(U"~9223372036854775807 % -1") == int_lit("0"));
    }

    // 一条孤零零的字面量表达式不会被 StaticEvaler 触碰（fold() 不处理裸字面量，只处理运算节点），
    // 所以下面这些用例都套一层 `+ 0`，逼折叠器真的走 node_to_int64 那条按值展开的路径，
    // 不能直接 fold_json(U"5e0") 后拿字面量原样的 raw_
    // 文本去比对——那测的是"没被碰过"，不是"按值算对了"
    TEST_CASE("科学计数法写法的 int 按值展开参与运算，不按字面文本") {
        CHECK(fold_json(U"1e2 + 1") == int_lit("101"));
        CHECK(fold_json(U"1e9 * 1e9") == int_lit("1000000000000000000"));
        CHECK(fold_json(U"0e0 + 5") == int_lit("5"));
        CHECK(fold_json(U"5e0 + 0") == int_lit("5"));
    }

    // 折叠器内部的指数展开上限（nMaxIntScientificExponent，约 19）纯粹是提前退出的效率阈值：
    // 超过它必然装不下 int64_t（int64_t 至多 19 位十进制数字）。尾数全 0 的例外不受这条上限约束。
    TEST_CASE("科学计数法指数在展开上限内、且展开结果没超出 int64_t，才折") {
        // 尾数 1 位 + 指数 18 个 0 == 19 位数字，恰好没超 INT64_MAX
        CHECK(fold_json(U"9e18 + 0") == int_lit("9000000000000000000"));
    }

    TEST_CASE("科学计数法展开后超出 int64_t，不折（不管是被上限直接挡住还是展开后才发现溢出）") {
        // 指数 19 没超折叠器内部上限，展开成 20 位数字，尝试塞进 int64_t 时才发现溢出
        CHECK(fold_json(U"1e19 + 0")["type"] == "OpBinary");
        // 指数 20 已经超过折叠器内部上限，直接跳过展开，不折
        CHECK(fold_json(U"1e20 + 0")["type"] == "OpBinary");
    }

    TEST_CASE("尾数为 0 的科学计数法恒折成 0，不受指数上限约束") {
        CHECK(fold_json(U"0e9999 + 0") == int_lit("0"));
        CHECK(fold_json(U"-0e9999") == int_lit("0"));
    }
}
