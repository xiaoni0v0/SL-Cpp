// AST 位置字段。默认的 JSON dump 不含位置，这里直接读节点。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("位置追踪——AstNode::pos_ 基本行为") {

    TEST_CASE("单行：pos_ 是节点自己的起始位置") {
        const AstNodePtr node{parse_single(U"x")};
        CHECK(node->pos_.row == 1);
        CHECK(node->pos_.col == 1);
    }

    TEST_CASE("前导空格：pos_ 跳过空白，从实际 token 开始算") {
        const AstNodePtr node{parse_single(U"  x")};
        CHECK(node->pos_.col == 3);
    }

    TEST_CASE("多条顶层表达式：行号正确递增") {
        const AstNodeProgramPtr program{parse_as_file(U"x = 1\ny = 2")};
        REQUIRE(program->exprs_.size() == 2);
        CHECK(program->exprs_[0]->pos_.row == 1);
        CHECK(program->exprs_[1]->pos_.row == 2);
        CHECK(program->exprs_[1]->pos_.col == 1);
    }

    TEST_CASE("跨行合并的表达式：pos_ 是合并前第一个 token 的位置") {
        // "d = e +\nf" 合并成一条表达式，整体 pos_ 应该是 'd' 的位置，
        // 内层 OpBinary(e, f) 的 pos_ 应该是 'e' 的位置（不是 'f' 换行后的位置）
        const AstNodePtr node{parse_single(U"d = e +\nf")};
        const auto *assign{dynamic_cast<AstNodeAssign *>(node.get())};
        REQUIRE(assign != nullptr);
        CHECK(assign->pos_.row == 1);
        CHECK(assign->pos_.col == 1); // 'd'

        const auto *add{dynamic_cast<AstNodeOpBinary *>(assign->value_.get())};
        REQUIRE(add != nullptr);
        CHECK(add->pos_.row == 1);
        CHECK(add->pos_.col == 5); // 'e'：d(1) (2)= =(3) (4)e(5)
        CHECK(add->pos_op_.row == 1);
        CHECK(add->pos_op_.col == 7); // '+': e(5) (6)+(7)
    }
}

TEST_SUITE("位置追踪——运算符自己的位置") {

    TEST_CASE("二元运算符 pos_op_ 跟整个节点的 pos_（左操作数起始位置）不同") {
        const AstNodePtr node{parse_single(U"a + b")};
        const auto *bin{dynamic_cast<AstNodeOpBinary *>(node.get())};
        REQUIRE(bin != nullptr);
        CHECK(bin->pos_.col == 1);    // 'a'
        CHECK(bin->pos_op_.col == 3); // '+': a(1) (2)+(3)
    }

    TEST_CASE("前缀一元运算符：pos_ 和 pos_op_ 重合（运算符本身就是起点）") {
        const AstNodePtr node{parse_single(U"-x")};
        const auto *un{dynamic_cast<AstNodeOpUnary *>(node.get())};
        REQUIRE(un != nullptr);
        CHECK(un->pos_.col == 1);
        CHECK(un->pos_op_.col == 1);
    }

    TEST_CASE("后缀 ?/!：pos_ 是操作数起始位置，pos_op_ 是问号/感叹号自己的位置") {
        const AstNodePtr node{parse_single(U"x?")};
        const auto *un{dynamic_cast<AstNodeOpUnary *>(node.get())};
        REQUIRE(un != nullptr);
        CHECK(un->pos_.col == 1);    // 'x'
        CHECK(un->pos_op_.col == 2); // '?'
    }

    TEST_CASE("复合赋值 pos_op_ 是 op= 自己的位置") {
        const AstNodePtr node{parse_single(U"x += 1")};
        const auto *ca{dynamic_cast<AstNodeCompoundAssign *>(node.get())};
        REQUIRE(ca != nullptr);
        CHECK(ca->pos_.col == 1);    // 'x'
        CHECK(ca->pos_op_.col == 3); // '+=': x(1) (2)+(3)=(4)
    }

    TEST_CASE("属性访问 pos_dot_ 是 '.' 自己的位置") {
        const AstNodePtr node{parse_single(U"x.attr")};
        const auto *attr{dynamic_cast<AstNodeAttr *>(node.get())};
        REQUIRE(attr != nullptr);
        CHECK(attr->pos_.col == 1);     // 'x'
        CHECK(attr->pos_dot_.col == 2); // '.'
    }

    TEST_CASE("调用 args_.pos_paren_ 是 '(' 自己的位置") {
        const AstNodePtr node{parse_single(U"f(1)")};
        const auto *call{dynamic_cast<AstNodeCall *>(node.get())};
        REQUIRE(call != nullptr);
        CHECK(call->pos_.col == 1);             // 'f'
        CHECK(call->args_.pos_paren_.col == 2); // '('
    }

    TEST_CASE("import 调用形态 args_.pos_paren_ 是 '(' 自己的位置，pos_ 是 'import' 的位置") {
        const AstNodePtr node{parse_single(U"import('math')")};
        const auto *call{dynamic_cast<AstNodeImportCall *>(node.get())};
        REQUIRE(call != nullptr);
        CHECK(call->pos_.col == 1);             // 'import'
        CHECK(call->args_.pos_paren_.col == 7); // '('
    }

    TEST_CASE("eval args_.pos_paren_ 是 '(' 自己的位置，pos_ 是 'eval' 的位置") {
        const AstNodePtr node{parse_single(U"eval('x')")};
        const auto *ev{dynamic_cast<AstNodeEval *>(node.get())};
        REQUIRE(ev != nullptr);
        CHECK(ev->pos_.col == 1);             // 'eval'
        CHECK(ev->args_.pos_paren_.col == 5); // '('
    }

    TEST_CASE("import 关键字形态 pos_ 是 'import' 的位置，不是第一段名字的位置") {
        const AstNodePtr node{parse_single(U"  import os.path")};
        const auto *kw{dynamic_cast<AstNodeImportKw *>(node.get())};
        REQUIRE(kw != nullptr);
        CHECK(kw->pos_.col == 3); // 'import'
    }

    TEST_CASE("索引 pos_bracket_ 是 '[' 自己的位置") {
        const AstNodePtr node{parse_single(U"a[0]")};
        const auto *index{dynamic_cast<AstNodeIndex *>(node.get())};
        REQUIRE(index != nullptr);
        CHECK(index->pos_.col == 1);         // 'a'
        CHECK(index->pos_bracket_.col == 2); // '['
    }

    TEST_CASE("链式调用/索引/属性访问，每一层各自的运算符位置都正确") {
        // "obj.attr[0]" -> obj(1)(2)(3)(4).attr[0]
        // o(1)b(2)j(3).(4)a(5)t(6)t(7)r(8)[(9)0(10)](11)
        const AstNodePtr node{parse_single(U"obj.attr[0]")};
        const auto *index{dynamic_cast<AstNodeIndex *>(node.get())};
        REQUIRE(index != nullptr);
        CHECK(index->pos_bracket_.col == 9); // '['
        const auto *attr{dynamic_cast<AstNodeAttr *>(index->object_.get())};
        REQUIRE(attr != nullptr);
        CHECK(attr->pos_dot_.col == 4); // '.'
    }
}

TEST_SUITE("位置追踪——链式比较 / is 链每个运算符自己的位置") {

    TEST_CASE("链式比较：positions_op_ 跟 ops_ 一一对应，各自指向自己的运算符") {
        // "a < b <= c" -> a(1) (2)<(3) (4)b(5) (6)<(7)=(8) (9)c(10)
        const AstNodePtr node{parse_single(U"a < b <= c")};
        const auto *cmp{dynamic_cast<AstNodeCompare *>(node.get())};
        REQUIRE(cmp != nullptr);
        REQUIRE(cmp->positions_op_.size() == 2);
        CHECK(cmp->positions_op_[0].col == 3); // 第一个 '<'
        CHECK(cmp->positions_op_[1].col == 7); // '<='
    }

    TEST_CASE("is 链：positions_op_ 指向每个 'is' 自己的位置") {
        // "a is b is c" -> a(1) (2)i(3)s(4) (5)b(6) (7)i(8)s(9) (10)c(11)
        const AstNodePtr node{parse_single(U"a is b is c")};
        const auto *is_node{dynamic_cast<AstNodeIs *>(node.get())};
        REQUIRE(is_node != nullptr);
        REQUIRE(is_node->positions_op_.size() == 2);
        CHECK(is_node->positions_op_[0].col == 3); // 第一个 'is'
        CHECK(is_node->positions_op_[1].col == 8); // 第二个 'is'
    }
}

TEST_SUITE("位置追踪——装饰器链每个 '@' 自己的位置") {

    TEST_CASE("紧邻 func 的装饰器链：positions_decorator_ 跟 decorators_ 一一对应") {
        // "@d1 @d2 func f() {}" -> @(1)d(2)1(3) (4)@(5)d(6)2(7) (8)f(9)...
        const AstNodePtr node{parse_single(U"@d1 @d2 func f() {}")};
        const auto *func_node{dynamic_cast<AstNodeFunc *>(node.get())};
        REQUIRE(func_node != nullptr);
        REQUIRE(func_node->positions_decorator_.size() == 2);
        CHECK(func_node->positions_decorator_[0].col == 1); // 第一个 '@'
        CHECK(func_node->positions_decorator_[1].col == 5); // 第二个 '@'
        // 整个节点的 pos_ 也应该从第一个 '@' 算起，而不是 'func' 自己的位置
        CHECK(func_node->pos_.col == 1);
    }

    TEST_CASE("紧邻 class 的装饰器链同理") {
        const AstNodePtr node{parse_single(U"@d1 @d2 class C {}")};
        const auto *cls{dynamic_cast<AstNodeClass *>(node.get())};
        REQUIRE(cls != nullptr);
        REQUIRE(cls->positions_decorator_.size() == 2);
        CHECK(cls->positions_decorator_[0].col == 1);
        CHECK(cls->positions_decorator_[1].col == 5);
        CHECK(cls->pos_.col == 1);
    }

    TEST_CASE("没有装饰器时，positions_decorator_ 为空，节点自身位置从关键字算起") {
        const AstNodePtr node{parse_single(U"func f() {}")};
        const auto *func_node{dynamic_cast<AstNodeFunc *>(node.get())};
        REQUIRE(func_node != nullptr);
        CHECK(func_node->positions_decorator_.empty());
        CHECK(func_node->pos_.col == 1); // 'func' 自己
    }
}
