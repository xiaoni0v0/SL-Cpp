#pragma once

#include <string>

/**
 * 判断字符是否为数字字符
 * @param c 字符
 * @return  是否为数字字符
 */
bool is_digit(char c);
bool is_digit(char32_t c);

/**
 * 判断字符是否为字母字符（仅限英文字母）
 * @param c 字符
 * @return  是否为字母字符
 */
bool is_alpha(char c);
bool is_alpha(char32_t c);

/**
 * 判断字符是否为字母字符（仅限英文字母）或数字字符
 * @param c 字符
 * @return  是否为字母字符
 */
bool is_alpha_digit(char c);
bool is_alpha_digit(char32_t c);

/**
 * UTF-8 -> UTF-32
 * 严格校验：非法续字节、overlong 编码、代理区 (U+D800~U+DFFF)、超出 U+10FFFF 均抛异常
 * @param utf8      UTF-8  字符串
 * @param file_path 文件路径，用于错误信息
 * @return          UTF-32 字符串
 */
std::u32string utf8_to_u32(const std::string &utf8, const std::string &file_path = "<unknown>");
std::u32string utf8_to_u32(char utf8, const std::string &file_path = "<unknown>");

/**
 * UTF-32 -> UTF-8
 * @param utf32     UTF-32 字符串
 * @param file_path 文件路径，用于错误信息
 * @return          UTF-8  字符串
 */
std::string u32_to_utf8(const std::u32string &utf32, const std::string &file_path = "<unknown>");
std::string u32_to_utf8(char32_t utf32, const std::string &file_path = "<unknown>");
