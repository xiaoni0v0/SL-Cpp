# Parser.cpp：finish_* 函数的 paren_depth_ 管理约定

Parser.cpp 里所有处理 `()`/`[]` 配对的 `finish_*` 函数（`finish_call`、`finish_index`、
`finish_func_params`、`finish_func_captures`）统一约定：

1. **起始括号永远由调用方消耗**，`finish_*` 函数从"起始括号已经被消耗"这个状态开始执行；
2. **`paren_depth_` 的增减完全由 `finish_*` 函数自己管理**——函数一进来就 `paren_depth_++`，收尾前
   `paren_depth_--`，调用方不用也不该碰 `paren_depth_`；
3. **收尾时永远是"先消耗收尾括号、再减 `paren_depth_`"**（`expect(close); paren_depth_--;`），跟
   开头"先消耗起始括号、再加 `paren_depth_`"（`expect(open); paren_depth_++;`）对称——不要写成
   `paren_depth_--; expect(close);` 这种反过来的顺序。

`finish_dict` 是唯一的例外：`{}` 的深度由 `parse_brace` 整体管理（进入时把 `paren_depth_` 存到局部
变量并清零、退出时恢复——这是因为 `{}` 开启的是全新的语句语境，跟"多一层括号嵌套"是不同的机制，详见
`parser/Parser.cpp` 里 `parse_brace` 的实现注释和 `.ai/context.md` 里对应的设计记录），`finish_dict`
完全不碰 `paren_depth_`。

## why

这套约定统一之前，`finish_call`/`finish_index` 是"调用方在调用前 `paren_depth_++`、函数自己只减不
增"，`finish_func_params`/`finish_func_captures` 是"函数自己全权管理"，两种写法混用；同时约一半的收尾
代码是"先减后消耗"、另一半是"先消耗后减"。这两处不统一都是用户明确要求理顺的（"要统一一下"）。选
"函数自己全权管理 + 消耗在前"这一版是因为：(a) 这样每个 `finish_*` 函数是完全自洽的单元，调用方不用
记住"调用我之前你还得自己 `++`"这种隐藏契约；(b) 跟开头"消耗在前、`++`在后"的顺序对称一致；(c)
`expect()` 抛异常时 Parser 整体废弃（不捕获继续解析），所以两种收尾顺序在行为上完全等价，纯粹是风格
选择，选对称的那个更好记。

## how to apply

以后往 Parser.cpp 加新的、处理括号配对的 `finish_*` 函数，直接照抄这个约定：调用方只消耗起始括号，
函数自己管 `paren_depth_` 的加减，收尾顺序是"先 `expect(close)` 再 `--`"。
