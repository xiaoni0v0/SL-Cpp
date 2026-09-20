# assert 还是 throw InternalError

**判据只有一个：Release（`NDEBUG`）下这一行被编译掉，接下来会发生什么？**

| 会发生什么 | 用什么 |
| --- | --- |
| 静默往下走，产出一个看着正常但是错的结果（错字节码、错数值、僵尸对象、越界读） | `throw InternalError` |
| 立刻崩溃，或者错得没法忽略 | `assert` |

`assert` 是"调用方保证 X"的开发期兜底，`NDEBUG` 下整行消失，所以**它不能承担真正的校验**（总原则见
[architecture.md](../architecture.md)「工程原则」）。`InternalError` 是 Release 也生效的校验，表示
"实现自己有 bug"，永远不转成 SL 异常（见 [cpp-layer-vs-sl-layer.md](cpp-layer-vs-sl-layer.md)）。

## 该 throw 的典型位置

- **switch 穷举完之后的兜底**。函数后面还得 `return` 一个值，这时 `assert(!"...")` 加一个假返回值就是
  上表第一行：Release 下悄悄返回假值。写 `throw InternalError{"OpCode: invalid value"}`，不写假返回值，也不
  写"调用方保证不会走到"再返回一个占位值。
- **条件取决于被编译的程序**（SL 源码、字节码的内容），而不是代码本身的结构。测试盖不住所有输入，
  违反一次就是一份错误的产物。例：`CodeBuilder` 里标签处栈深不一致、操作数栈弹空、跳转指向没放置过的
  标签。
- **将来可能来自不可信的地方的值**（`eval`、加载字节码得到的 `OpCode`）。
- **静默产生僵尸/脏状态**：`Heap::collect()` 第 5 步"引用计数必须恰好是 1"。

## 该 assert 的典型位置

- **只取决于调用点怎么写的 API 误用**（`emit` 传错了重载、`place_label` 放了两次），任何一条走到它的
  测试在 Debug 下都会撞上。
- **入口处已经做过真校验之后，内部反复检查它的推论**：`Parser::parse_program` 入口用 `InternalError`
  校验过"token 列表非空、恰好一个 `END_OF_FILE` 且在末尾"，所以 `peek`/`advance` 里的
  `assert(pos_ < tokens_.size())` 是推论，不必再抛。**边界上校验一次（throw），里面复检用 assert。**
- **热路径和算法内核的不变量**：`numeric/` 里 `BigInt`/`BigDec`/`dec_math` 的 `assert`（比如大小相减前
  `assert(compare_magnitude(a, b) >= 0)`）、以后的主循环分派、引用计数增减。这些即使 Release 下违反也
  是静默出错，但多一次比较在热路径上不值得，由差分测试兜底。
- 除零、空指针解引用这类违反了会立刻崩溃的前提。

## 不属于这两者的

- 用户 SL 代码写错了：抛 `SyntaxError`（或以后的 SL 异常），不是 `InternalError`。
- **`numeric/` 只依赖最底层的 `common/`，不依赖 `cppexceptions/`**，所以不能抛 `InternalError`。需要 Release 也生效的校验时用标准库异常：
  `std::invalid_argument`（调用方传了不合法的参数）、`std::logic_error`（枚举值越界这类"不可能"）。

## 措辞

- `assert` 的说明**可以不写**：表达式自己读得懂就不写（`assert(index < names_.size())`）。要写就写
  英文、很短：`assert(cond && "use emit_jump")`。`assert(!"msg")` 只用于"走到这里本身就是错"的无条件
  分支，而且后面不该再跟一个假返回值（那就该 throw）。
- 抛出去的 message（`InternalError`、`std::logic_error` 等）一律 `Module: message`，格式与措辞见
  [user-facing-strings.md](user-facing-strings.md)：`Module` 是类或子系统名，不是函数名
  （`OpCode: invalid value`，不是 `opcode_name: OpCode out of range`）。**`assert` 不加前缀**——它自己会
  打出文件和行号。位置信息不进 message。
- 测试按 message 的**子串**匹配（`test/parser/01_literals/literal_raw_test.cpp`），改措辞前先 grep
  `test/`。

## Why

`assert` 让人误以为"这里有防护"，Release 下其实什么都没有；而 Release 才是真正会跑用户程序的构建。
静默产出错误的字节码/数值，比崩溃更难查。反过来，把热路径和已经在边界上校验过的推论也统统换成
`throw`，只是给每次调用多付一次比较，什么都没多防住。所以取舍看"违反之后是响还是哑"，不看"这条检查
重不重要"。
