# 纯 C++ 层 / SL 层的分界：谁能命名 SL 异常

**能命名 SL 异常类、能碰对象模型和 GC 的最低一层，是"内置类型/内置操作的实现"和 `compiler/` 门面。
再往下的模块只报告物理失败，不做语义判断。**

| 层                                            | 出错时怎么办                                       |
|-----------------------------------------------|----------------------------------------------------|
| `numeric/`（`BigInt`/`BigDec`/`DecContext`）  | 抛 C++ 异常，或返回 `optional`。不认识 SL          |
| `utils/`                                      | 同上                                               |
| `compiler/` 的 `lexer` `parser` `analyzer`     | 抛 C++ `SyntaxError`/`InternalError`。不认识 SL    |
| `int`/`decimal`/`str`… 等内置类型的实现       | catch 下层的 C++ 异常 → 抛 SL 异常                 |
| `compiler/codegen`、`compiler/` 门面          | 同上（`codegen` 要造真 SL 对象，本来就是 SL 层）   |

注意 **分界是按模块划的，不是按目录**：`compiler/` 里 `lexer`/`parser`/`analyzer` 是纯 C++ 层，
`codegen` 是 SL 层。宿主异常类型本身住在顶层的 `cpp_exceptions/`，因为 `utils/` 也要用（它是纯 C++ 层，
不能反过来依赖 `compiler/`）。

## 为什么不让底层直接抛 SL 异常

"给运行时开个 `raise_xxx()` 门面，`BigInt` 直接调"这个方案被否决过。编译期耦合确实很浅，问题在别处：

- **同一个底层失败，SL 层的含义不唯一，抛出点没有那个信息。** `BigInt::pow` 指数为负抛
  `domain_error`，但 SL 里 `int ** 负数` 是 decimal 不是错误——真触发了说明调用方违约，该
  `InternalError`；`from_decimal_string` 语法不合法，从 `decimal('abc')` 来该抛
  `decimal.ConversionSyntax`，从 lexer 拿已校验的字面量来却该是 `InternalError`。抛出点只知道"物理上
  出了什么事"，只有调用方知道"这对 SL 程序意味着什么"。
- **这些模块会在根本没有 SL 程序的场景下跑**：`numeric/`、`lexer` 各有独立的测试目标，只链自己那几个
  `.cpp`。一旦调运行时门面，它们就得链进对象模型 + GC + bootstrap，或者维护一份测不到真实路径的桩。

## 怎么转

**优先"就近判断"而不是"就近捕获"**——把前置检查提到知道语义的那一层，通常连 try/catch 都不需要：

```cpp
// int 的 __op_floordiv__
if (rhs.is_zero()) return raise_zero_division("integer division or modulo by zero");
return make_int(lhs.floor_div(rhs));   // 前置条件已保证，不会抛
```

底层报告"预期内的失败"时，比起抛 C++ 异常更适合返回 `optional`/`expected`（`BigDec::try_from_string`
已经是这个形态），把决策权交给调用方。C++ 异常留给真正的契约违反。

主循环那层 `catch` 是 **兜底**，不是通用转换器：兜到未知 C++ 异常 = 某个内置操作漏了自己该做的转换 =
VM 自己的 bug，一律 `InternalError`，不让 SL 代码捕获。

## 两个例外

- **`InternalError` 永远不转**，一路穿到顶层。它是唯一没有对应 SL 类、明确不可被 SL 捕获的异常；
  转了的话 `eval("...")` 就能把编译器自身的 bug 当正常控制流捕获掉。
- **`compiler/` 门面转 `SyntaxError` 是无条件的**，冷启动和 `eval` 走同一段代码：顶层把未捕获的 SL
  异常打印出来，`eval` 则让它照常沿 SL 帧栈传播。`SL.md`「SyntaxError 在编译期抛出；其他所有异常均在
  运行时抛出」说的就是这个 SL 层的 `SyntaxError`。

## 宿主异常存字段，不存拼好的字符串

`cpp_exceptions/` 里每个异常都把**结构化字段**留着，`what()` 只是构造时由字段渲染出来的一种呈现：

| 异常 | 字段 |
|---|---|
| `SyntaxError` | `location()`（`SourceLocation`：file/row/col）、`message()` |
| `InternalError` | `location()` 是 `optional`——运行期的内部错误没有"源码哪一行"、`message()` |
| `EncodingError` | `file_path()`、`offset()`（**字节偏移**，出错时字节流还没切成行）、`message()` |
| `FileNotFoundError` | `path()`、`message()` |

**Why**：这些异常大多要在某个边界上转成 SL 异常对象，转换方要的是分开的 file/row/col/message。只留
一个拼好的串，等于逼转换方去反解析自己刚拼出来的东西。而且同一个错误有多个消费方——冷启动打 stderr、
`eval` 里变成 SL 对象、以后 REPL/IDE 要结构化位置——在抛出点烙死一种呈现，等于替所有消费方做了决定。

**How to apply**：新增宿主异常类型时，按"这个错误天然带哪些定位信息"设计字段，不要为了统一硬套
`SourceLocation`（`EncodingError` 就只有字节偏移，`FileNotFoundError` 连位置都没有）。渲染在构造函数
里做一次给 `what()`，格式改动是纯呈现层的事，不影响转换方。

## 转换的具体实现在 `runtime/HostErrorConversion.{h,cpp}`

这条约定不是只停在文档层面：`SyntaxError`/`EncodingError`/`FileNotFoundError` 各有一个
`exception_from(...)` 函数，把宿主异常的字段翻成对应 SL 类的 `BaseException` 对象（`.args` 塞
`message()`，不带 `what()` 那层 file:row:col 装饰）。**`InternalError` 没有对应函数、以后也不会有**
——它没有 SL 类，这条转换对它不成立，见 [object-model-conventions.md](object-model-conventions.md)
「异常」一节。
