# 写 codegen 测试：期望值用 raw string，以及折叠器剪枝这个坑

`test/codegen/**` 的每个用例都是"编译一段源码 → 反汇编成文本 → 比对"。虚拟机还没有，这是验证
codegen 的唯一手段，所以这套写法会一直用下去。

## 期望值一律写成 raw string literal

```cpp
CHECK(codegen_instrs(U"x = 1") == R"(LOAD_CONST 0
COPY 1
STORE_NAME 0
RETURN_VALUE
)");
```

**不要写成字符串数组再拼**（`instrs({"LOAD_CONST 0", "COPY 1", ...})` 这种）。试过，被 clang-format
排成了没法看的阶梯状——它会把长初始化列表按括号深度层层缩进，一条十来行的期望值糊成半屏。raw string
内部 clang-format 一个字都不动，排版才稳得住；而且那块文本跟反汇编输出长得一模一样，对照着读不用在
脑子里再做一次拼接。

末尾那个换行是 `disassemble_instrs` 自己带的，别漏。

## 单独一条纯字面量语句会被折叠器剪掉

`ExprFolder` 的 Program 剪枝把纯字面量当死代码去掉，所以下面这些**到不了 codegen**：

```cpp
codegen_instrs(U"[1, 2]")   // Program 空了，只剩一条 LOAD_CONST None
codegen_instrs(U"(1, 2)")
codegen_instrs(U"None")
```

要让字面量活到 codegen，写成 `x = 字面量` 再取一次 `x`：

```cpp
codegen_instrs(U"x = [1, 2]\nx")
```

同理，想让某个节点在 `want_value=false` 下编译（测"没人要值时省掉哪些指令"），得在它后面再跟一条
**非字面量**语句垫底——尾随一个 `1` 会被剪掉，尾随一个 `x` 才留得住。

## 一个用例起落一次运行时

`RuntimeFixture` 在 `test_utils.h` 里。codegen 要把真 SL 对象放进常量表，运行时必须先在；而
`Runtime::shutdown()` 会 `Heap::collect()`，所以**手上的 `Ref<Code>` 必须先于 fixture 析构**——
声明顺序反了就是访问已释放内存。`codegen_instrs`/`codegen_dump` 已经处理好，自己另写辅助时留意。

## 两种反汇编形态各管一头

- `disassemble_instrs`：一行一条指令，`EXTENDED_ARG` 前缀折进参数。**精确比对用这个**，测试写的是
  逻辑指令流，不受参数宽度变化影响。
- `disassemble`：完整版，含四张表和嵌套 `Code`。查常量表/名字表/捕获标记用它，配
  `constants_of()`、`section_of()` 抽某一段。
