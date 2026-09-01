// 跟 Python int 的交叉验证。表由 gen_big_int_cases.py 生成，不要手改 .inc。
#include "test_utils.h"

#include <doctest/doctest.h>
#include <cstdlib>

namespace {
#include "../big_int_cases.inc"
} // namespace


TEST_SUITE("BigInt——跟 Python int 的交叉验证") {

    TEST_CASE("加减乘") {
        for (const char *const raw : kAddCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK((d(f[0]) + d(f[1])).to_decimal_string() == f[2]);
        }
        for (const char *const raw : kSubCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK((d(f[0]) - d(f[1])).to_decimal_string() == f[2]);
        }
        for (const char *const raw : kMulCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK((d(f[0]) * d(f[1])).to_decimal_string() == f[2]);
        }
    }

    TEST_CASE("floor_div / mod（Python 的 // 和 % 恰好也是向负无穷取整，语义天然一致）") {
        for (const char *const raw : kFloorDivCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK(d(f[0]).floor_div(d(f[1])).to_decimal_string() == f[2]);
        }
        for (const char *const raw : kModCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK(d(f[0]).mod(d(f[1])).to_decimal_string() == f[2]);
        }
    }

    TEST_CASE("位运算（Python 的 &/|/^ 同样按无穷位补码语义）") {
        for (const char *const raw : kAndCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK((d(f[0]) & d(f[1])).to_decimal_string() == f[2]);
        }
        for (const char *const raw : kOrCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK((d(f[0]) | d(f[1])).to_decimal_string() == f[2]);
        }
        for (const char *const raw : kXorCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK((d(f[0]) ^ d(f[1])).to_decimal_string() == f[2]);
        }
    }

    TEST_CASE("移位（a << k 恒等于 a*2^k，a >> k 恒等于 a//2^k，Python 同样按此定义）") {
        for (const char *const raw : kShiftLeftCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK((d(f[0]) << std::stoll(f[1])).to_decimal_string() == f[2]);
        }
        for (const char *const raw : kShiftRightCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK((d(f[0]) >> std::stoll(f[1])).to_decimal_string() == f[2]);
        }
    }

    TEST_CASE("pow") {
        for (const char *const raw : kPowCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            CHECK(d(f[0]).pow(d(f[1])).to_decimal_string() == f[2]);
        }
    }

    TEST_CASE("比较") {
        for (const char *const raw : kCompareCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 3);
            const BigInt a{d(f[0])}, b{d(f[1])};
            const int expect{std::stoi(f[2])};
            CHECK((a < b) == (expect < 0));
            CHECK((a == b) == (expect == 0));
            CHECK((a > b) == (expect > 0));
        }
    }

    TEST_CASE("科学计数法解析（期望值是 Python 的 int(尾数) * 10**指数）") {
        for (const char *const raw : kSciNotationCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 2);
            CHECK(d(f[0]).to_decimal_string() == f[1]);
        }
    }

    TEST_CASE("bit_length") {
        for (const char *const raw : kBitLengthCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 2);
            CHECK(d(f[0]).bit_length() == static_cast<size_t>(std::stoull(f[1])));
        }
    }

    TEST_CASE("num_decimal_digits（期望值是 Python 的 len(str(abs(a)))）") {
        for (const char *const raw : kDecimalDigitsCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 2);
            CHECK(d(f[0]).num_decimal_digits() == static_cast<size_t>(std::stoull(f[1])));
        }
    }

    TEST_CASE("to_double（期望值是 Python float(a) 的精确十六进制表示，strtod 认得同一种记法）") {
        for (const char *const raw : kToDoubleCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 2);
            CHECK(d(f[0]).to_double() == std::strtod(f[1].c_str(), nullptr));
        }
    }
}
