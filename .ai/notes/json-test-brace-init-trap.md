# 测试里 parse_json 结果的花括号初始化陷阱

在 `test/parser/**` 里凡是要把 `parse_json(...)`/`parse_program_json(...)` 的返回值存进一个局部
变量、之后还要对它做 `["field"]` 多次取值的，必须写成 `const auto x = parse_json(source);`（拷贝
初始化，用 `=`），不能写成 `const auto x{parse_json(source)};`（花括号初始化）。

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
方向上又踩了一次（`combination_test.cpp` 里）。

## how to apply

- `const auto x = parse_json(...);` / `const auto x = parse_program_json(...);` —— 必须用 `=`。
- `const auto &y{x["field"]};` —— 绑定已存在对象的子字段是**引用**，不走构造函数，花括号没问题，
  这种可以继续用 `{}`。
- 判断标准就是"右边是不是在构造一个新的 `nlohmann::json`/`nlohmann::ordered_json` 对象"：是则必须
  `=`；只是引用绑定或者右边本来就不是 json 类型（比如 `std::u32string`）则不受影响。
