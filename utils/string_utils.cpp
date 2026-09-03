#include "string_utils.h"

#include "../diagnostics/EncodingError.h"

#include <cstdint>
#include <string>

bool is_digit(const char c) { return c >= '0' && c <= '9'; }

bool is_digit(const char32_t c) { return c >= U'0' && c <= U'9'; }

bool is_alpha(const char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

bool is_alpha(const char32_t c) { return (c >= U'A' && c <= U'Z') || (c >= U'a' && c <= U'z'); }

bool is_alpha_digit(const char c) { return is_digit(c) || is_alpha(c); }

bool is_alpha_digit(const char32_t c) { return is_digit(c) || is_alpha(c); }

std::u32string utf8_to_u32(const std::string &utf8, const std::string &file_path) {
    std::u32string result;
    result.reserve(utf8.size()); // 上界，避免多次扩容

    const unsigned char *const base{reinterpret_cast<const unsigned char *>(utf8.data())};
    const unsigned char *p{base};
    const unsigned char *end{p + utf8.size()};

    while (p < end) {
        uint32_t cp;
        int extra; // 续字节个数

        const size_t pos{static_cast<size_t>(p - base)}; // 当前序列起始字节下标
        const unsigned char b{*p++};
        if (b < 0x80) {
            // 0xxxxxxx
            result.push_back(b);
            continue;
        }
        if ((b & 0xE0) == 0xC0) {
            // 110xxxxx
            cp = b & 0x1F;
            extra = 1;
        } else if ((b & 0xF0) == 0xE0) {
            // 1110xxxx
            cp = b & 0x0F;
            extra = 2;
        } else if ((b & 0xF8) == 0xF0 && b <= 0xF4) {
            // 11110xxx
            cp = b & 0x07;
            extra = 3;
        } else {
            throw EncodingError{file_path, pos, "utf8_to_u32: invalid lead byte"};
        }

        if (end - p < extra) {
            throw EncodingError{file_path, pos, "utf8_to_u32: truncated sequence"};
        }

        for (int i = 0; i < extra; ++i) {
            const unsigned char c{*p++};
            // 必须是 10xxxxxx
            if ((c & 0xC0) != 0x80) {
                throw EncodingError{
                    file_path,
                    static_cast<size_t>(p - base - 1),
                    "utf8_to_u32: invalid continuation byte"
                };
            }
            cp = (cp << 6) | (c & 0x3F);
        }

        // 拒绝 overlong 编码
        static constexpr uint32_t min_cp[4]{0, 0x80, 0x800, 0x10000};
        if (cp < min_cp[extra]) {
            throw EncodingError{file_path, pos, "utf8_to_u32: overlong encoding"};
        }

        // 拒绝代理区和越界码点
        if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
            throw EncodingError(file_path, pos, "utf8_to_u32: invalid code point");
        }

        result.push_back(static_cast<char32_t>(cp));
    }
    return result;
}

std::u32string utf8_to_u32(const char utf8, const std::string &file_path) {
    return utf8_to_u32(std::string(1, utf8), file_path);
}

std::string u32_to_utf8(const std::u32string &utf32, const std::string &file_path) {
    std::string result;
    result.reserve(utf32.size() * 4); // 上界，避免多次扩容

    for (size_t i = 0; i < utf32.size(); ++i) {
        const uint32_t cp{utf32[i]};

        if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
            throw EncodingError{file_path, i, "u32_to_utf8: invalid code point"};
        }

        if (cp < 0x80) {
            result.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return result;
}

std::string u32_to_utf8(const char32_t utf32, const std::string &file_path) {
    return u32_to_utf8(std::u32string(1, utf32), file_path);
}
