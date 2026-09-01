// 跟 CPython decimal 的交叉验证。表由 gen_big_dec_cases.py 生成，不要手改 .inc。
#include "test_utils.h"

#include <doctest/doctest.h>

namespace {
#include "../big_dec_cases.inc"
} // namespace

TEST_SUITE("BigDec——跟 CPython decimal 的交叉验证") {

    TEST_CASE("加法") {
        for (const char *const raw : kAddCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).add(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("减法") {
        for (const char *const raw : kSubCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).sub(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("乘法") {
        for (const char *const raw : kMulCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).mul(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("除法") {
        for (const char *const raw : kDivCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).div(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("//（期望值是按向负无穷重新推的，不是 Python 的 //）") {
        for (const char *const raw : kFloorDivCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).floor_div(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("%（同上）") {
        for (const char *const raw : kModCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).mod(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("divmod（期望值取 // 和 % 各自的结果，flags 取并集）") {
        for (const char *const raw : kDivmodCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 5);
            DecContext ctx{quiet_context()};
            const auto [quotient, remainder]{d(f[0]).divmod(d(f[1]), ctx)};
            CHECK(quotient.to_string() == f[2]);
            CHECK(remainder.to_string() == f[3]);
            CHECK(flags_to_string(ctx.flags()) == f[4]);
        }
    }

    TEST_CASE("八种舍入方式 × prec 1/2/3/7") {
        for (const char *const raw : kRoundCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 5);
            DecContext ctx{quiet_context(std::stoi(f[1]), rounding_from_name(f[2]))};
            CHECK(d(f[0]).plus(ctx).to_string() == f[3]);
            CHECK(flags_to_string(ctx.flags()) == f[4]);
        }
    }

    TEST_CASE("一元 - 和 abs × 八种舍入 × prec 1/2/3/7") {
        for (const char *const raw : kUnaryCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 6);
            DecContext ctx{quiet_context(std::stoi(f[2]), rounding_from_name(f[3]))};
            const BigDec value{d(f[0])};
            const BigDec got{f[1] == "minus" ? value.minus(ctx) : value.abs(ctx)};
            CHECK(got.to_string() == f[4]);
            CHECK(flags_to_string(ctx.flags()) == f[5]);
        }
    }

    TEST_CASE("指数边界：Overflow / Underflow / Subnormal / Clamped") {
        for (const char *const raw : kEdgeCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 9);
            DecContext ctx{quiet_context(
                std::stoi(f[3]), rounding_from_name(f[4]), std::stoi(f[5]), std::stoi(f[6])
            )};
            const BigDec a{d(f[0])};
            const BigDec b{d(f[1])};
            const std::string &op{f[2]};
            const BigDec result{
                op == "mul"        ? a.mul(b, ctx)
                : op == "div"      ? a.div(b, ctx)
                : op == "add"      ? a.add(b, ctx)
                : op == "sub"      ? a.sub(b, ctx)
                : op == "floordiv" ? a.floor_div(b, ctx)
                                   : a.mod(b, ctx)
            };
            REQUIRE_MESSAGE(
                (op == "mul" || op == "div" || op == "add" || op == "sub" || op == "floordiv" ||
                 op == "mod"),
                "用例表里出现了没实现的 op"
            );
            CHECK(result.to_string() == f[7]);
            CHECK(flags_to_string(ctx.flags()) == f[8]);
        }
    }

    TEST_CASE("随机操作数 × 随机 op/prec/rounding") {
        for (const char *const raw : kMixedCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 7);
            DecContext ctx{quiet_context(std::stoi(f[3]), rounding_from_name(f[4]))};
            const BigDec a{d(f[0])};
            const BigDec b{d(f[1])};
            const std::string &op{f[2]};
            const BigDec result{
                op == "add"        ? a.add(b, ctx)
                : op == "sub"      ? a.sub(b, ctx)
                : op == "mul"      ? a.mul(b, ctx)
                : op == "div"      ? a.div(b, ctx)
                : op == "floordiv" ? a.floor_div(b, ctx)
                                   : a.mod(b, ctx)
            };
            REQUIRE_MESSAGE(
                (op == "add" || op == "sub" || op == "mul" || op == "div" || op == "floordiv" ||
                 op == "mod"),
                "用例表里出现了没实现的 op"
            );
            CHECK(result.to_string() == f[5]);
            CHECK(flags_to_string(ctx.flags()) == f[6]);
        }
    }

    TEST_CASE(
        "陷阱开启：抛不抛、抛哪个条件、抛之前 flags 走到哪一步（上面所有表都只有陷阱全关"
        "的路径，这张表专补陷阱开着时的行为，值池里塞了带非零指数的零和极端指数）"
    ) {
        // 行格式：a|b|op|prec|rounding|emax|emin|trap|结果|flags——真抛了的话结果为空、
        // flags 是 "THROW:条件名;抛出时已记下的flags"
        const auto run_trapped{
            [](const BigDec &a, const BigDec &b, const std::string &op, DecContext &ctx) {
                try {
                    const BigDec r{
                        op == "add"        ? a.add(b, ctx)
                        : op == "sub"      ? a.sub(b, ctx)
                        : op == "mul"      ? a.mul(b, ctx)
                        : op == "div"      ? a.div(b, ctx)
                        : op == "floordiv" ? a.floor_div(b, ctx)
                        : op == "mod"      ? a.mod(b, ctx)
                        : op == "pow"      ? a.pow(b, ctx)
                        : op == "sqrt"     ? a.sqrt(ctx)
                        : op == "exp"      ? a.exp(ctx)
                        : op == "ln"       ? a.ln(ctx)
                                           : a.log10(ctx)
                    };
                    return std::pair{r.to_string(), flags_to_string(ctx.flags())};
                } catch (const DecTrapped &e) {
                    return std::pair{
                        std::string{},
                        "THROW:" + std::string{dec_condition_name(e.condition())} + ";" +
                            flags_to_string(ctx.flags())
                    };
                }
            }
        };

        const auto check_trap_table{[&run_trapped](const auto &table, const bool unary) {
            for (const char *const raw : table) {
                const std::string line{raw};
                CAPTURE(line);
                const std::vector<std::string> f{split_fields(line)};
                REQUIRE(f.size() == 10);
                const DecRounding rounding{rounding_from_name(f[4])};
                DecContext ctx{
                    quiet_context(std::stoi(f[3]), rounding, std::stoi(f[5]), std::stoi(f[6]))
                };
                ctx.traps().add(condition_from_name(f[7]));
                const BigDec a{d(f[0])};
                const BigDec b{unary && f[1].empty() ? a : d(f[1])};
                const auto [res, fl]{run_trapped(a, b, f[2], ctx)};
                CHECK(res == f[8]);
                CHECK(fl == f[9]);
                // 抛出来的 DecTrapped 在 BigDec 一侧就该是细分条件（0/0 是 DivisionUndefined，
                // 不是折算后的 InvalidOperation）——生成器那边按这个约定出的题
            }
        }};

        check_trap_table(kTrappedArith, false);
        check_trap_table(kTrappedFloorDivMod, false);
        check_trap_table(kTrappedTrans, true);
    }

    TEST_CASE("陷阱开启：== 与序比较抛不抛、抛什么") {
        // 行格式：a|b|trap|eq;eqf|rel;orf，';' 前是结果、后是 flags（抛了就是
        // "THROW:条件名;flags"）
        for (const char *const raw : kTrappedCmp) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 5);
            const BigDec a{d(f[0])};
            const BigDec b{d(f[1])};
            DecContext ctx{quiet_context()};
            ctx.traps().add(condition_from_name(f[2]));
            try {
                const bool eq{a.equals(b, ctx)};
                const std::string got{
                    (eq ? "1" : "0") + std::string{";"} + flags_to_string(ctx.flags())
                };
                CHECK(got == f[3]);
            } catch (const DecTrapped &e) {
                CHECK(
                    "THROW:" + std::string{dec_condition_name(e.condition())} + ";" +
                        flags_to_string(ctx.flags()) ==
                    f[3]
                );
            }
            try {
                const std::partial_ordering ordering{a.compare_ordering(b, ctx)};
                const std::string rel{
                    ordering == std::partial_ordering::unordered ? "un"
                    : ordering == std::partial_ordering::less    ? "lt"
                    : ordering == std::partial_ordering::greater ? "gt"
                                                                 : "eq"
                };
                CHECK(rel + ";" + flags_to_string(ctx.flags()) == f[4]);
            } catch (const DecTrapped &e) {
                CHECK(
                    "THROW:" + std::string{dec_condition_name(e.condition())} + ";" +
                        flags_to_string(ctx.flags()) ==
                    f[4]
                );
            }
        }
    }

    TEST_CASE("幂运算 **") {
        for (const char *const raw : kPowCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 8);
            DecContext ctx{quiet_context(
                std::stoi(f[2]), rounding_from_name(f[3]), std::stoi(f[4]), std::stoi(f[5])
            )};
            CHECK(d(f[0]).pow(d(f[1]), ctx).to_string() == f[6]);
            CHECK(flags_to_string(ctx.flags()) == f[7]);
        }
    }

    TEST_CASE("超越函数 sqrt/exp/ln/log10") {
        for (const char *const raw : kTranscendentalCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 8);
            const DecRounding rounding{rounding_from_name(f[3])};
            DecContext ctx{
                quiet_context(std::stoi(f[2]), rounding, std::stoi(f[4]), std::stoi(f[5]))
            };
            const BigDec a{d(f[0])};
            const std::string &op{f[1]};
            const BigDec result{
                op == "sqrt"  ? a.sqrt(ctx)
                : op == "exp" ? a.exp(ctx)
                : op == "ln"  ? a.ln(ctx)
                              : a.log10(ctx)
            };
            CHECK(result.to_string() == f[6]);
            CHECK(flags_to_string(ctx.flags()) == f[7]);
            // 这四个内部会临时把舍入方式换成 HalfEven，算完必须还回去
            CHECK(ctx.rounding() == rounding);
        }
    }

    TEST_CASE("比较") {
        for (const char *const raw : kCompareCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 6);
            const BigDec a{d(f[0])};
            const BigDec b{d(f[1])};

            DecContext eq_ctx{quiet_context()};
            CHECK(a.equals(b, eq_ctx) == (f[3] == "1"));
            CHECK(flags_to_string(eq_ctx.flags()) == f[4]);

            DecContext ord_ctx{quiet_context()};
            const std::partial_ordering ordering{a.compare_ordering(b, ord_ctx)};
            const std::string relation{
                ordering == std::partial_ordering::unordered ? "un"
                : ordering == std::partial_ordering::less    ? "lt"
                : ordering == std::partial_ordering::greater ? "gt"
                                                             : "eq"
            };
            CHECK(relation == f[2]);
            CHECK(flags_to_string(ord_ctx.flags()) == f[5]);
        }
    }
}
