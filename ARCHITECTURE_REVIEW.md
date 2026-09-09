# codegen review 导览

> 这份文件是给你 review 用的临时导览，**随时可删**。里面所有该长期留存的结论都已经写进
> `compiler/codegen/bytecode.md`、`.ai/architecture.md`、`.ai/context.md`、`.ai/notes/` 了，
> 删掉这份不会丢任何东西。

## 这轮加了什么

```
compiler/codegen/
  x_opcodes.inc          53 条指令：枚举名 / 助记符 / 参数种类 / 顺序流栈效应
  x_unary_ops.inc        UNARY_OP 的 6 个参数
  x_binary_ops.inc       BINARY_OP 的 20 个参数（and/or 不在里面，走短路）
  opcode.h               由上面三份展开出 OpCode / UnaryOp / BinaryOp / ArgKind
                         + stack_effect() + stack_effect_extra_on_jump() + is_terminator()
  CodeBuilder.{h,cpp}    一份 Code 的组装器（约 230 行）
  CodeGen.{h,cpp}        AST → 指令（约 1400 行）
  disassembler.{h,cpp}   Code → 文本

runtime/
  x_internal_objects.inc  LOAD_INTERNAL 那张全局共享表
  x_singletons.inc        六个单例（这轮顺手统一的）
  Runtime.{h,cpp}         三张根表形状统一

compiler/Compiler.{h,cpp} 加了 compile() 三个重载
executor/Executor.cpp     起落 Runtime、打印反汇编
test/codegen/             9 个文件，95 个用例
```

## 三层怎么分的

|                | 管什么                                              | 不管什么                               |
| -------------- | --------------------------------------------------- | -------------------------------------- |
| `CodeGen`      | 哪个节点发哪些指令                                  | 指令怎么编码、跳转目标是多少           |
| `CodeBuilder`  | 标签回填、`EXTENDED_ARG` 定点迭代、栈深跟踪、四张表 | AST 长什么样（**完全不 include AST**） |
| `disassembler` | `Code` → 文本                                       | 别的                                   |

分开的实际收益：加一条语法只动 `CodeGen`，改指令编码只动 `CodeBuilder`。嵌套作用域就是 builder
栈上多压一层。

## 最值得挑刺的地方

按"出错了最难查"排的：

1. **`CodeGen.cpp` 的 `gen_target_prefix` / `gen_target_store` / `gen_store_one`**（赋值那一组）。
   `under_value` 那个参数是全文件最绕的地方：顶层赋值是"先压目标前置项、再压值"，而解构的元素赋值
   反过来（值已经由 `UNPACK` 产出在栈顶），前置项要靠 `INSERT 2` 逐个垫到它下面。
   对照 `SL.md` 3.3 第 6 条读。

2. **`gen_unwind` / `gen_unwind_with_value`**（跳出去那一组）。这轮刚修过一个 bug（见下），
   `POP_BLOCK`、裁栈、`CALL_FINALLY`、`None` 占位四件事的顺序都有讲究。

3. **`visit(AstNodeTry)`**。`finally` 只编译一份，四条路径进去时栈顶必须都是"这条路径要带出去的那个
   值"——`bytecode.md` 那一节讲了为什么必须这样。

4. **`visit(AstNodeForCond)` / `visit(AstNodeForIter)`**。结果槽 / 迭代器 / 本轮的值三者的栈序，
   以及 `break` 和 `continue` 各自裁到哪个深度。迭代模式的 `continue_depth_` 比 `break_depth_` 深
   一层（多个迭代器），这是 `FOR_ITER` 要求迭代器在栈顶逼出来的。

5. **`CodeBuilder::build()` 的定点迭代**。补前缀会把跳转目标推远、可能逼出新前缀，反复扫到不变为止。
   `widths` 只增不减所以必收敛——如果哪天改成可增可减就会死循环。

## 这轮修掉的三个语义理解错

留在这里是想让你知道**我曾经在哪些地方读错过 SL.md**，同类地方值得多看两眼：

1. **Program 的值**：一度按复合表达式处理（值 = 最后一条），实际上 Program 的值只由 `return` 决定，
   走到末尾就是 `None`。`func f() { 1 }` 返回 `None` 不是 `1`。
2. **类的求值顺序**：一度把捕获压在基类之前，实际 `SL.md` 3.4.8 规定先求基类、建帧、再处理捕获。
   于是 `MAKE_CLASS` 的栈序是 `bases captures doc name`，跟函数那边（捕获最先）不对称。
3. **跳出 `try` 漏 `POP_BLOCK`**：`break`/`continue`/`return` 跨出 `try` 时没弹运行期块栈，跳到外面
   之后再抛异常会撞进失效的处理器；`return` 更糟，不先弹自己这层就进 `finally` 体，体内抛异常会被
   自己接住转圈。

前两个各有专门的回归用例，第三个在 `try_test.cpp` 最后两个 `TEST_CASE`。

## 怎么自己看产物

```bash
./cmake-build-debug/SL.exe 某个.sl
```

会依次打 token、两遍 AST、完整反汇编（四张表 + 指令 + 嵌套 `Code`）。

测试里两种形态各管一头：`codegen_instrs()` 一行一条指令（精确比对用），`codegen_dump()` 是完整版
（查常量表/名字表/捕获标记用）。`codegen_body()` 再去掉每份 Program 末尾那条隐含的 `return None`。

## 已知没做的

- **`eval` 编译不出东西**：`CodeGen::generate` 只收 `AstNodeProgram`，而 `eval` 要编译的是"恰好一条
  表达式"。等虚拟机落地时补一个单表达式入口。
- **`LOAD_INTERNAL` 的 `ImportFunc` 是个占位对象**：函数对象类型还没有，现在那个槽放的是个同名的
  `NamedSingleton`。字节码形状是对的，等 `Function` 落地换掉。
- **窥孔优化一概没做**（`bytecode.md` 明说不做）。所以你会看到 `JUMP` 到紧邻下一条这种废指令，
  比如 `if (c) { x }` 在没人要值时那条跳过空 else 的 `JUMP`。
- **不可达代码照常生成**。循环体恒 `break` 时，后面的收尾指令仍然发出来（栈跟踪已停，不影响正确性）。
- **`Compiler` 门面还没把 `SyntaxError` 转成 SL 异常对象**，等帧栈和主循环。
