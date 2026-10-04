# codegen 架构

> 临时导览，**随时可删**。内容在 `compiler/codegen/bytecode.md`、`.ai/architecture.md`、
> `.ai/context.md` 里都有，删掉不丢东西。

## 文件

```
compiler/codegen/
  x_opcodes.inc          指令清单（唯一真源）：枚举名 / 助记符 / 参数种类 / 顺序流栈效应
  x_unary_ops.inc        UNARY_OP 的参数清单
  x_binary_ops.inc       BINARY_OP 的参数清单
  opcode.h               由三份 inc 展开：OpCode / UnaryOp / BinaryOp / ArgKind、
                         stack_effect() / stack_effect_extra_on_jump() / is_terminator()
  ConstPool.{h,cpp}      常量表去重（按结构标识）
  CodeBuilder.{h,cpp}    一份 Code 的组装器
  CodeGen.{h,cpp}        AST → 指令
  disassembler.{h,cpp}   Code → 文本
  bytecode.md            指令语义、栈约定、各语法的编译模式
```

入口只有一个：`CodeGen::generate_program(root, file_path)`，外面再由 `Compiler::compile` 包一层。

## 三层

|                | 管什么                                               | 不管什么                      |
| -------------- | ---------------------------------------------------- | ----------------------------- |
| `CodeGen`      | 哪个节点发哪些指令；循环、`try`、作用域的上下文      | 指令怎么编码、跳转目标落在哪  |
| `CodeBuilder`  | 伪指令流、标签回填、`EXTENDED_ARG`、栈深跟踪、四张表 | AST 节点（只用到 `Position`） |
| `disassembler` | `Code` → 文本                                        | —                             |

按变化原因切的：加语法只动 `CodeGen`，改指令编码只动 `CodeBuilder`。反汇编器单独一层，是因为虚拟机
没落地之前，它是验证 codegen 的唯一手段。

## `CodeBuilder`

**伪指令 + 标签**。发指令时跳转目标只写标签号，`build()` 时统一回填。

**`EXTENDED_ARG` 定点迭代**。参数超过一个字节要补前缀，补前缀会让后面的指令右移、可能把某个跳转
推远到需要更多前缀。所以反复"算偏移 → 看每条跳转要几个字节"直到不变。每条指令的宽度只增不减，必收敛。

**静态栈深跟踪**。每发一条指令按 `stack_effect()` 记账，顺带算出 `max_stack`：

- 跳转指令：跳过去那条路径的栈深 = 顺序流栈深 + `stack_effect_extra_on_jump()`（只有 `FOR_ITER`
  和两条 `SETUP_*` 不是 0）。
- 标签记着"该处应有的栈深"。`emit_jump` 和 `place_label` 两头核对，第一次得知就记下，对不上直接断言。
- 终止指令（`JUMP`/`RETURN_VALUE`/`RAISE` 等）之后标为不可达，不可达的指令照发但不记账，直到下一个标签。
- 字节码**不必**以终止指令结尾：顺序流走出末尾等价于 `return None`，跳转目标也可以恰好等于指令总数。
  `build()` 只核对一条——走出末尾时栈必须是空的。

栈不平衡是手写字节码最容易犯的错，而没有虚拟机时跑不出来，所以在这里当场抓。

## `CodeGen`

### 核心不变量：`want_value`

```cpp
void gen(const AstNode &node, bool want_value);
```

编译完一个节点，栈深恰好增加 `want_value ? 1 : 0`（除非这之后不可达）。`gen()` 在每次分派前后断言
这一条，于是每个 `visit` 只需要对自己负责。`want_value` 经成员 `want_value_` 传给 `visit`。

两套规则别混：

- **复合表达式**（`gen_exprs`）：值是最后一条，只有最后一条跟着外层的 `want_value`。
- **Program**（`gen_program`，文件 / 函数体 / 类体）：值只由 `return` 决定，每一条都 `want_value=false`，
  不补任何收尾指令，走出字节码末尾就是 `return None`，由虚拟机兑现。

### 三种上下文

```
builders_   作用域栈，栈顶是正在填的 Code。函数体、类体各压一层
loops_      循环栈：break/continue 的标签、各自该裁到的栈深、进循环时外面有几层块
blocks_     运行期块栈的编译期镜像：SETUP_EXCEPT / SETUP_FINALLY 各一项
```

`begin_code()` 压一个新 builder，同时把 `loops_`/`blocks_` 整个换成空的存进 `saved_*`——函数体里的
`break` 跳不到外层循环。`end_code()` 反过来，并把 builder `build()` 成 `Code`，调用方再
`add_nested_code` 进外层表。

### 跳出去：`gen_unwind` / `gen_unwind_with_value`

块栈是**运行期**的，静态跳转不会自动弹。所以 `break`/`continue`/`return` 离开 `try` 时由内到外逐层：

1. `POP_BLOCK`；
2. 如果这层是 `finally`：把栈裁到进 `try` 时的深度，`CALL_FINALLY` 走一遍 `finally` 体。

区别只在栈顶带什么：`break`/`continue` 压 `None` 占位、回来弹掉，最后裁到循环要求的深度；`return`
带着返回值一路穿过去（用 `INSERT` 把它垫到多余项下面再弹多余项）。

### `finally` 只编译一份

`CALL_FINALLY` 的返回地址存在帧自己的小栈 `ByteCodeFrame::returns_` 上，不占操作数栈。于是四条进
`finally` 的路径（正常结束 / `return` / 异常 / `break`·`continue`）进去时栈上一律是"入口深度 + 1"，
栈顶就是这条路径要带出去的值，`finally` 体只需一份，`END_FINALLY` 按返回地址回去。

### 分组

`CodeGen.h` 按注释分段，每段一组 `gen_*` 辅助：

| 分组       | 负责                                                          |
| ---------- | ------------------------------------------------------------- |
| 小零件     | 常量、`None`、裁栈、条件跳转、两个 unwind                     |
| 容器       | list/tuple/dict，有 `*`/`**` 展开时先建空再逐项并入           |
| 比较链     | 比较链与 `is` 链共用，同一个运算符连续应用                    |
| 赋值       | 目标前置操作数 / 写入 / 解构递归 / 复合赋值读旧值             |
| 调用       | `CALL` / `CALL_KW` / `CALL_EX` 三选一                         |
| 循环       | 结果槽（计数 / `$` / `$$`）与每轮折入                         |
| 嵌套作用域 | 捕获登记与求值、装饰器、形参形状、没写的槽位压 `Missing` 哨兵 |

赋值那组的 `under_value` 参数最绕：顶层赋值先压前置项再压值；解构元素的值已由 `UNPACK` 产出在栈顶，
前置项得用 `INSERT` 垫到它下面。

函数和类的建立顺序不对称，是 SL.md 规定的：

```
MAKE_FUNC   captures params ret_type doc name   （捕获最先）
MAKE_CLASS  bases captures doc name             （基类先求值、建帧，再处理捕获）
```

## `disassembler`

- `disassemble()`：完整版，四张表 + 指令 + 递归打嵌套 `Code`。`Executor` 用它。
- `disassemble_instrs()`：一行一条指令，测试里精确比对用。

## 测试

`test/codegen/`，辅助在 `test_utils.h`：

- `codegen_instrs()` / `codegen_dump()`：分别对应上面两种反汇编。
- 被测表达式放赋值右侧，才是 `want_value=true`。
- 期望值用 raw string literal（clang-format 不动它内部）。
