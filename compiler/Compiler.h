#pragma once

#include "lexer/token.h"
#include "parser/ast_nodes/ast_nodes.h"

#include <functional>
#include <string>
#include <vector>

/**
 * compiler/ 的对外接口，把 lexer -> parser -> analyzer 串成一条流水线
 */
class Compiler final {
  public:
    /**
     * 各阶段刚跑完时的旁路观察点，给开发期打印中间产物用；留空的就不观察。
     */
    struct Hooks {
        std::function<void(const std::vector<Token> &tokens)> after_tokenize;
        std::function<void(const AstNodeProgram &ast)> after_parse;
        std::function<void(const AstNodeProgram &ast)> after_analyze;
    };

    Compiler() = delete;
    ~Compiler() = delete;
    Compiler(const Compiler &) = delete;
    Compiler(Compiler &&) = delete;
    Compiler &operator=(const Compiler &) = delete;
    Compiler &operator=(Compiler &&) = delete;

    /**
     * 编译一份源码
     * @param source    源码
     * @param file_path 报错信息里显示的路径
     * @param hooks     可选的阶段观察点
     */
    [[nodiscard]] static AstNodeProgramPtr compile_source(
        std::u32string source, std::string file_path = "<unknown>", const Hooks &hooks = {}
    );

    /**
     * 读一个文件并编译它。读不出来抛 FileNotFoundError，不是合法 UTF-8 抛 EncodingError
     */
    [[nodiscard]] static AstNodeProgramPtr
    compile_file(const std::string &file_path, const Hooks &hooks = {});
};
