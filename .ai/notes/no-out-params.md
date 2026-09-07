# 不用「引用形参改外边」，把结果 return 出来

**规则**：辅助函数不要收一个 `T &`（或指针）当"就地改掉的输出参数"，把算出来的东西 return 出来。
需要连着揉进多个值，就写成变参模板或者在调用点反复赋值，不要退回去改引用。

**why**：调用点看不出哪个实参会被改。`hash_combine(seed, x);` 这种签名，读的人必须跳进定义才知道
`seed` 是被改的那个，而 `seed = hash_combine(seed, x)` 一眼可见。副作用藏在形参里还让函数用不了
临时对象、也进不了常量表达式。

**how to apply**：`compiler/codegen/ConstPool.cpp` 里的 `hash_combine` 是现成的样子——变参模板
收 seed 的**副本**，折叠表达式在这个副本上揉，最后 return：

```cpp
template <std::convertible_to<std::size_t>... Values>
std::size_t hash_combine(std::size_t seed, const Values... values);
```

于是「一次揉四个字段」写成一句 `return hash_combine(seed, a, b, c, d);`，循环里连揉写成
`hash = hash_combine(hash, x)`——那是对局部变量赋值，不是输出参数，不在这条规则的范围里。

**边界**：真正的 in-out 语义不算违反——成员函数改自己的状态、访问者按
[visitor-result-passing.md](visitor-result-passing.md) 的成员通道往回写结果、需要同时返回"成功与否"
和"值"的 `try_*(out)` 形状，都保留。这条针对的是**本来就能 return 却设计成改实参**的辅助函数。
