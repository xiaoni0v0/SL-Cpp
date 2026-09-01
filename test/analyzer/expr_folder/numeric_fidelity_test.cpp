// StaticEvaler：数值折叠的**保真性**——折出来的值必须和运行期逐位一致。
//
// 这个文件专门钉住两类曾经真出过错的地方，它们的共同点是"折了，但折出来的是错值"，
// 而不是"漏折"。漏折只是少优化一次，折错值是静默改变程序语义，危害完全不同一个量级。
//
// 一、int 的科学计数法写法（1e2、0e0）
//     `raw_` 存的是源码原样，`1e2` 和 `100` 是同一个值的两种写法。凡是拿 `raw_` 当数字串
//     直接比长度/字典序，或者"解析失败就当它很大"的地方，都会算错。曾经的实际后果：
//     `0e0` 的真值被判成真，连带死分支消除留下了本该被消掉的那一支。
//
// 二、decimal 一律不折——算术、比较、真值，一概不折
//     decimal 的值依赖运行期上下文（prec/rounding），编译期算出来的东西不保证跟运行期一致。
//     早先版本让"比较不舍入，照折"，用 double 实现时确实出过错（1.0e-400 下溢成 0、
//     9007199254740993 和 ...92.0 被 double 的 53 位尾数挤成同一个值）；现在的结论更简单：
//     既然折叠器压根不打算引入高精度库去精确处理 decimal，就干脆一条规则到底，
//     decimal 什么都不折，不再区分"这一种运算安全、那一种不安全"。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 保真性——int 的科学计数法写法") {

    TEST_CASE("按值参与相等比较，不按 raw_ 的字面文本") {
        CHECK(fold_json(U"1e2 == 100") == bool_lit(true));
        CHECK(fold_json(U"1e9 == 1000000000") == bool_lit(true));
        CHECK(fold_json(U"0e0 == 0") == bool_lit(true));
        CHECK(fold_json(U"0e5 == 0") == bool_lit(true));
        CHECK(fold_json(U"1e2 == 101") == bool_lit(false));
        // 指数写不写 '+' 不影响值
        CHECK(fold_json(U"1e9 == 1e+9") == bool_lit(true));
    }

    TEST_CASE("按值参与大小比较——光比字符串长度/字典序会全错") {
        CHECK(fold_json(U"1e1 < 11") == bool_lit(true));  // 10 < 11
        CHECK(fold_json(U"2e2 < 1e3") == bool_lit(true)); // 200 < 1000
        CHECK(fold_json(U"1e0 > 0") == bool_lit(true));   // "1e0" 比 "0" 长，但值只是 1
        CHECK(fold_json(U"0e0 > 0") == bool_lit(false));  // 值是 0，不大于 0
        CHECK(fold_json(U"0e0 >= 0") == bool_lit(true));
    }

    TEST_CASE("真值按值判零，0e0 是假") {
        CHECK(fold_json(U"not 0e0") == bool_lit(true));
        CHECK(fold_json(U"not 1e0") == bool_lit(false));
        CHECK(fold_json(U"0e0 and 5") == int_lit("0e0")); // and 取左（假），且原样保留写法
        CHECK(fold_json(U"0e0 or 5") == int_lit("5"));    // or 左假取右
    }

    // 这条是整个文件里最要紧的：真值判错会让死分支消除**留下错误的那一支**，
    // 把 111/222 换成有副作用的表达式就是实打实的语义改变
    TEST_CASE("真值判零正确，死分支消除才会挑对分支") {
        CHECK(fold_json(U"if (0e0) 111 else 222") == int_lit("222"));
        CHECK(fold_json(U"if (1e0) 111 else 222") == int_lit("111"));
        CHECK(fold_json(U"if (0e5) 111 else 222") == int_lit("222"));
    }

    TEST_CASE("参与算术时按值展开") {
        CHECK(fold_json(U"1e2 + 1") == int_lit("101"));
        CHECK(fold_json(U"1e2 * 1e2") == int_lit("10000"));
        CHECK(fold_json(U"1e2 // 3") == int_lit("33"));
        CHECK(fold_json(U"~0e0") == int_lit("-1"));
    }
}

TEST_SUITE("StaticEvaler 保真性——decimal 一律不折") {

    // 折了就会得到 0.30000000000000004 这个 double 凑出来的错值；SL.md 4.2.6 说 decimal
    // 底数是 10，`0.1 + 0.2 == 0.3` 为真，运行期算出来是精确的 0.3
    TEST_CASE("decimal 算术一律不折，避免造出运行期不会出现的值") {
        CHECK(fold_json(U"0.1 + 0.2")["type"] == "OpBinary");
        CHECK(fold_json(U"1.5 - 1.4")["type"] == "OpBinary");
        CHECK(fold_json(U"1.5 * 2.0")["type"] == "OpBinary");
        CHECK(fold_json(U"1 / 3")["type"] == "OpBinary");
        CHECK(fold_json(U"2 ** 0.5")["type"] == "OpBinary");
        CHECK(fold_json(U"-1.5")["type"] == "OpUnary");
    }

    // 掺 decimal 的 0 ** 0 运行期触发 decimal.InvalidOperation（默认上下文里是陷阱），
    // 折成 1.0 等于把一个必然发生的异常吞掉了
    TEST_CASE("0 ** 0 掺了 decimal 时不折，不吞掉运行期的 InvalidOperation") {
        CHECK(fold_json(U"0.0 ** 0")["type"] == "OpBinary");
        CHECK(fold_json(U"0 ** 0.0")["type"] == "OpBinary");
        CHECK(fold_json(U"0 ** 0") == int_lit("1")); // 对照：两边都是 int 时是 1，照折
    }

    TEST_CASE("decimal 之间的比较一律不折，不管值多普通") {
        CHECK(fold_json(U"9007199254740993 == 9007199254740992.0")["type"] == "Compare");
        CHECK(fold_json(U"1.0e-400 != 0.0")["type"] == "Compare");
        CHECK(fold_json(U"1.0000000000000000000000000001 == 1.0")["type"] == "Compare");
        CHECK(fold_json(U"1.5 < 2.5")["type"] == "Compare");
        CHECK(fold_json(U"1.5 == 1.50")["type"] == "Compare"); // 标度不同但值相等，folder 也不管
        CHECK(fold_json(U"0.0 == 0.00")["type"] == "Compare");
    }

    // 连带后果：decimal 的一元 +/- 本身也不折（同样是"值依赖运行期上下文"），
    // 所以负的 decimal（`-0.0`）在 AST 里是 OpUnary 套一个字面量，不是纯字面量本身；
    // 不过这跟"decimal 一律不折"是同一个结论的两个体现，不影响下面的比较照样不折
    TEST_CASE("负的 decimal 也不是纯字面量，比较/真值同样不折") {
        // 比较运算建的是 AstNodeCompare（链式比较自成一类），不是 AstNodeOpBinary
        CHECK(fold_json(U"0.0 == -0.0")["type"] == "Compare");
        CHECK(fold_json(U"-1.5 < 0.0")["type"] == "Compare");
        CHECK(fold_json(U"not -0.0")["type"] == "OpUnary");
    }

    TEST_CASE("int 与 decimal 跨类型比较也不折：认识对方，但折叠器不碰 decimal 的值") {
        CHECK(fold_json(U"1 == 1.0")["type"] == "Compare");
        CHECK(fold_json(U"1 < 1.5")["type"] == "Compare");
        CHECK(fold_json(U"True == 1.0")["type"] == "Compare"); // bool 一样不折
    }

    TEST_CASE("decimal 的真值一律不折，哪怕字面看着明显是 0") {
        CHECK(fold_json(U"not 0.0")["type"] == "OpUnary");
        CHECK(fold_json(U"not 0.00")["type"] == "OpUnary");
        CHECK(fold_json(U"not 1.0e-400")["type"] == "OpUnary");
        CHECK(fold_json(U"if (1.0e-400) 111 else 222")["type"] == "If");
    }
}

TEST_SUITE("StaticEvaler 保真性——bool 按 SL.md 4.2.5 折算成 int") {

    // bool 不继承 int，但 numbers.Real 要求的四则运算和大小比较由 bool 自己实现：
    // 参与运算前按 1/0 折算成 int 再复用 int 的实现，结果类型是 int（不是 bool）
    TEST_CASE("四则运算与比较里 bool 折算成 1/0，结果是 int") {
        CHECK(fold_json(U"True + True") == int_lit("2"));
        CHECK(fold_json(U"True * 5") == int_lit("5"));
        CHECK(fold_json(U"False - 1") == int_lit("-1"));
        CHECK(fold_json(U"True == 1") == bool_lit(true));
        CHECK(fold_json(U"False < 1") == bool_lit(true));
        CHECK(fold_json(U"True ** 3") == int_lit("1"));
    }

    // 位运算是 int 特有的，bool 没有这些方法，运行期是 TypeError——不能折，也不能提前报
    TEST_CASE("bool 参与位运算一律不折，把 TypeError 留给运行期") {
        CHECK(fold_json(U"True & 1")["type"] == "OpBinary");
        CHECK(fold_json(U"1 | True")["type"] == "OpBinary");
        CHECK(fold_json(U"True ^ False")["type"] == "OpBinary");
        CHECK(fold_json(U"True << 1")["type"] == "OpBinary");
        CHECK(fold_json(U"1 >> True")["type"] == "OpBinary");
        CHECK(fold_json(U"~True")["type"] == "OpUnary");
    }

    // 容器重复次数同理只认 int，bool 不算（表里 bool×str 落在 F 组）
    TEST_CASE("str 重复次数只认 int，bool 不算") {
        CHECK(fold_json(U"'ab' * True")["type"] == "OpBinary");
        CHECK(fold_json(U"'ab' * 2") == str_lit("abab")); // 对照
    }
}
