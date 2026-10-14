# 对外输出的字符串一律英文、且尽量短

代码注释用中文（`AGENTS.md` 的硬约束），**但凡是会离开源码、被人看到的字符串一律用英文**：

- 异常信息（`SyntaxError`/`InternalError`/`EncodingError`/…… 的 message 与 `what()` 渲染）
- `assert` 的那句说明（表达式自己读得懂就省掉，见 [assert-vs-internal-error.md](assert-vs-internal-error.md)）
- `std::cout`/`std::cerr` 打出去的任何东西（用法提示、崩溃兜底、计时）
- CMake 的 `message(FATAL_ERROR ...)`
- 以后 SL 层异常对象的 `.args`、内置函数产生的报错

**不算对外输出、继续用中文**：注释、`.ai/` 与各种 `.md`、doctest 的 `TEST_CASE`/`SUBCASE` 名字
（那是给写代码的人看的，不进任何用户可见的输出）。

## 简洁的判据

**无歧义的前提下能少说就少说**，别写成句子，别加句号。

| 不要 | 要 |
| --- | --- |
| `运行时被初始化了两次` | `Runtime: double init` |
| `对一个引用计数已经为 0 的对象调用了 decref` | `decref on refcount 0` |
| `垃圾对象仍被非法引用` | `garbage object still referenced` |
| `分词器崩溃了: 未知错误` | `lexer crashed: unknown error` |

已有的行文风格以 `compiler/` 为准，照着写就行：小写开头、名词短语或"expected X"式的祈使描述、
不带句号——`break outside loop`、`'{}' is a reserved word`、`expected ']' to close list literal`。

## 格式：`Module: message`

**宿主层抛出的内部错误一律带 `Module: ` 前缀**（`InternalError`、`std::logic_error`、
`std::invalid_argument` 等）：`Runtime: double init`、`CodeBuilder: operand stack underflow`、
`OpCode: invalid value`、`BigInt::pow: negative exponent`。`InternalError` 没有 C++ 侧的调用栈，前缀
是定位"哪一块出的错"的唯一线索。

- `Module` 是**类名或子系统名**，不是函数名；同一个类里两条措辞会撞（`BigInt::from_decimal_string` 和
  `BigInt::pow` 都会说 `negative exponent`）时才写到 `Class::method`。
- 带位置的辅助函数（`CodeGen`/`Parser`/`SemanticChecker` 的 `error_internal`）在函数里统一补前缀，
  调用点只写 `unexpected null node` 这类不带前缀的话。
- **不加前缀的**：`SyntaxError`（语言层错误，已经有 `file:row:col` 和 `SyntaxError:`）、`assert`
  （自带文件行号）、CLI 的用法提示。

**位置信息不进 message**。file/row/col 是 `SLException` 子类的独立字段，`what()` 负责把它们渲染
成前缀，message 本身只说"哪里不对"——见 [cpp-layer-vs-sl-layer.md](cpp-layer-vs-sl-layer.md)。

## Why

SL 是给外人用的语言，报错是它的对外接口的一部分；报错语言跟着开发者的母语走，等于把接口绑死在
一个人身上。而且宿主层异常的 message 迟早要原样塞进 SL 异常对象的 `.args`（`HostErrorConversion`
已经这么干了），到那时它就不只是"给开发者看的日志"，而是 SL 程序能读到、能打印、能拿去匹配的值。
