# 测试里 json 返回值的花括号初始化陷阱

凡是要把某个返回 `nlohmann::json`/`ordered_json`（按值）的测试工具函数结果存进一个局部变量、
之后还要对它多次取值/断言的，必须写成 `const auto x = f(...);`（拷贝初始化，用 `=`），不能写成
`const auto x{f(...)};`（花括号初始化）。目前踩过这个坑的至少有两处：`test/parser/**` 的
`parse_json(...)`/`parse_program_json(...)`，以及 `test/analyzer/**` 的 `fold_json(...)`——
但凡以后再加新的"解析/折叠后转 json 方便断言"这类工具函数，同样的坑大概率还会在新目录下复现
一次，不是只有这两个函数名要小心。

## why

`nlohmann::json` 同时有拷贝/转换构造函数和 `initializer_list<json>` 构造函数，`T x{expr};` 在两者
都可行时优先选 `initializer_list` 版本——把 `{expr}` 当成"只有 1 个元素的列表"，再套用 nlohmann
"列表每一项是不是 `[字符串,值]` 二元数组"的启发式判断整体是不是对象；单元素列表里那一项是一个完整
对象（不是二元数组），于是整个列表被判成数组，变量实际变成 `[expr]`——`x["type"]` 这类取值会直接抛
`json.exception.type_error.305 cannot use operator[] with a string argument with array`，而且这个
错误发生在没被 `CHECK`/`REQUIRE` 包裹的裸语句里时，doctest 只会笼统报"test case THREW exception"，
不会精确指到出问题的那一行，容易误导排查方向。

这个坑在 `literals_test.cpp`/`func_test.cpp` 等文件里已经有注释提醒过（用于"expected 字面量"场景），
但只覆盖了 `nlohmann::json{...}` 手写期望值那个方向；在"直接对 `parse_json` 返回值本身取值"这个新
方向上又踩了一次（`combination_test.cpp` 里），后来在 `container_ops_test.cpp` 里对 `fold_json`
返回值也踩了第三次——排查那次花了很大力气：一度怀疑到生产代码的 `to_json()`/`to_json_impl()`
头上，往两处都加了 `fprintf`/`std::cerr` 调试输出逐层验证，才确认生产代码全程正确，问题只在测试
自己那行 `const auto j256{fold_json(...)};`。以后再见到"json 断言莫名其妙变成数组"，先检查这一条，
不用重新排查一遍。

## how to apply

- `const auto x = parse_json(...);` / `const auto x = parse_program_json(...);` /
  `const auto x = fold_json(...);` —— 必须用 `=`。
- `const auto &y{x["field"]};` —— 绑定已存在对象的子字段是**引用**，不走构造函数，花括号没问题，
  这种可以继续用 `{}`。
- 判断标准就是"右边是不是在构造一个新的 `nlohmann::json`/`nlohmann::ordered_json` 对象"：是则必须
  `=`；只是引用绑定或者右边本来就不是 json 类型（比如 `std::u32string`）则不受影响。
