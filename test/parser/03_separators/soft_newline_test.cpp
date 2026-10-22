// 在表达式片段边界插入换行，检查不同括号语境下的结构保持不变。
#include "../../../utils/string_utils.h"
#include "../test_utils.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

TEST_SUITE("软换行的跨语境组合") {

    TEST_CASE("各括号语境允许在表达式的每个片段边界插入空行") {
        const std::vector<std::vector<std::u32string>> expressions{
            {U"return"},
            {U"return", U"+", U"1"},
            {U"return", U"-", U"1"},
            {U"return", U"not", U"a", U"in", U"b"},
            {U"return", U"[", U"a", U",", U"*", U"rest", U"]"},
            {U"(", U"x", U":", U"const", U"T", U"=", U"value", U")"},
            {U"a", U"<", U"b", U"<=", U"c", U"is", U"d"},
            {U"a", U"..=", U"b", U"..", U"-", U"1"},
            {U"not", U"a", U"in", U"b", U"and", U"c", U"or", U"d"},
            {U"obj", U".", U"attr", U"[", U"]", U"(", U"1", U")", U"?"},
            {U"$", U"{", U"key", U"->", U"value", U",", U"**", U"rest", U"}"},
            {U"$", U"{", U"a", U",", U"*", U"rest", U"}"},
            {U"(", U"func typed[P]() { return P }", U")", U"(", U")"},
            {U"'text'"},
            {U"0.5e+123"}
        };
        const std::vector<std::pair<std::u32string, std::u32string>> contexts{
            {U"(", U")"},
            {U"[", U"]"},
            {U"obj[", U"]"},
            {U"f(", U")"},
            {U"${", U"}"},
            {U"${", U" -> value}"},
            {U"${key -> ", U"}"},
            {U"func f(x = ", U") {}"}
        };

        for (const auto &parts : expressions) {
            std::u32string expression;
            for (const auto &part : parts) {
                if (!expression.empty()) expression += U' ';
                expression += part;
            }
            for (const auto &[prefix, suffix] : contexts) {
                const auto expected = parse_json(prefix + expression + suffix);
                for (std::size_t gap{0}; gap <= parts.size(); ++gap) {
                    std::u32string source{prefix};
                    for (std::size_t i{0}; i <= parts.size(); ++i) {
                        if (i == gap) source += U"\n\n";
                        if (i < parts.size()) {
                            if (i != 0) source += U' ';
                            source += parts[i];
                        }
                    }
                    source += suffix;
                    INFO("source: " << u32_to_utf8(source));
                    CHECK(parse_json(source) == expected);
                }
            }
        }
    }
}
