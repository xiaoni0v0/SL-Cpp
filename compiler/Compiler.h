#pragma once

#include "../runtime/objects/Code.h"
#include "lexer/token.h"
#include "parser/ast_nodes/ast_nodes.h"

#include <string>
#include <vector>

/**
 * compiler/ 的对外接口。
 */
class Compiler final {
  public:
    Compiler() = delete;
    ~Compiler() = delete;
    Compiler(const Compiler &) = delete;
    Compiler(Compiler &&) = delete;
    Compiler &operator=(const Compiler &) = delete;
    Compiler &operator=(Compiler &&) = delete;

    // 读源码文件并解码。读不出来抛 FileNotFoundError，不是合法 UTF-8 抛 EncodingError
    [[nodiscard]] static std::u32string read_source(const std::string &file_path);

    // 跑完分词器为止
    [[nodiscard]] static std::vector<Token>
    tokenize(std::u32string source, std::string file_path = "<unknown>");

    // 跑完解析器为止
    [[nodiscard]] static AstNodeProgramPtr
    parse(std::vector<Token> tokens, std::string file_path = "<unknown>");
    [[nodiscard]] static AstNodeProgramPtr
    parse(std::u32string source, std::string file_path = "<unknown>");

    // 跑完分析器为止
    static void analyze(AstNodeProgram &ast, std::string file_path = "<unknown>");
    [[nodiscard]] static AstNodeProgramPtr
    analyze(std::vector<Token> tokens, std::string file_path = "<unknown>");
    [[nodiscard]] static AstNodeProgramPtr
    analyze(std::u32string source, std::string file_path = "<unknown>");

    // 跑到代码生成为止
    // 前提：Runtime 已经起来
    [[nodiscard]] static Ref<Code>
    compile(const AstNodeProgram &ast, std::string file_path = "<unknown>");
    [[nodiscard]] static Ref<Code>
    compile(std::vector<Token> tokens, std::string file_path = "<unknown>");
    [[nodiscard]] static Ref<Code>
    compile(std::u32string source, std::string file_path = "<unknown>");
};
