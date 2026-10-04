# 写 codegen 测试：期望值用 raw string，以及折叠器剪枝这个坑

`test/codegen/**` 的每个用例都是"编译一段源码 → 反汇编成文本 → 比对"。虚拟机还没有，这是验证
codegen 的唯一手段，所以这套写法会一直用下去。

## 期望值一律写成 raw string literal

```cpp
CHECK(codegen_instrs(U"x = 1") == R"(LOAD_CONST 0
STORE_NAME 0
)");
```

**不要写成字符串数组再拼**（`instrs({"LOAD_CONST 0", "COPY 1", ...})` 这种）。试过，被 clang-format
排成了没法看的阶梯状——它会把长初始化列表按括号深度层层缩进，一条十来行的期望值糊成半屏。raw string
内部 clang-format 一个字都不动，排版才稳得住；而且那块文本跟反汇编输出长得一模一样，对照着读不用在
脑子里再做一次拼接。

末尾那个换行是 `disassemble_instrs` 自己带的，别漏。

## 被测表达式放在赋值右侧

**Program 里每一条语句都是 `want_value=false`**（Program 的值只由 `return` 决定），所以顶层随手写
一个表达式测不到 `want_value=true` 那一路。要测它，把表达式放进赋值右侧：

```cpp
codegen_instrs(U"x = 1\ny = x + x")   // x + x 在 want_value=true 下编译
codegen_instrs(U"x = 1\nx + x")       // 裸表达式，走 want_value=false 那一路
```

赋值本身要 `want_value=true` 时，再套一层：`y = (x = 1)`，或者用复合表达式 `y = { x = 1 }`。

这个写法顺带解决另一个问题：**单独一条纯字面量语句会被折叠器的 Program 剪枝当死代码去掉**
（`[1, 2]`、`(1, 2)`、`None` 都到不了 codegen，Program 空了只剩一条 `LOAD_CONST None`），而
`x = [1, 2]` 里的 list 有赋值托着，剪不掉。

## Program 末尾没有收尾指令

不写 `return` 时字节码里没有 `LOAD_CONST <None>` + `RETURN_VALUE`——pc 走出末尾就是 `return None`，
所以 `codegen_instrs()` 的期望值直接就是语句本身的指令，常量表里也不会为此多出一个 `None`。
`program_value_test.cpp` 专门验这一点（含空 Program、跳转目标等于指令总数）。

## 一个用例起落一次运行时

`RuntimeFixture` 在 `test_utils.h` 里。codegen 要把真 SL 对象放进常量表，运行时必须先在；而
`Runtime::shutdown()` 会 `Heap::collect()`，所以**手上的 `Ref<Code>` 必须先于 fixture 析构**——
声明顺序反了就是访问已释放内存。`codegen_instrs`/`codegen_dump` 已经处理好，自己另写辅助时留意。

## 两种反汇编形态各管一头

- `disassemble_instrs`：一行一条指令，`EXTENDED_ARG` 前缀折进参数。**精确比对用这个**，测试写的是
  逻辑指令流，不受参数宽度变化影响。
- `disassemble`：完整版，含四张表和嵌套 `Code`。查常量表/名字表/捕获标记用它，配
  `constants_of()`、`section_of()` 抽某一段。
