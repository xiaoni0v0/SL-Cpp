# 怎么自己构建、跑测试

`E:\Programs\SL-Cpp\.ai\run_test.bat`（脚本本身也在项目里，随文件夹走）：内部自己设好 CLion 自带的
MinGW/CMake/Ninja 的 PATH，构建 `cmake-build-debug` 的 all 目标后跑 ctest。

**怎么调用**：在 Bash 工具里执行

```
cmd //c "E:\Programs\SL-Cpp\.ai\run_test.bat"
```

（Git Bash 下路径分隔要用 `cmd //c` 双斜杠，单斜杠 `cmd /c` 会被 Git Bash 自己的路径转换搞坏。）

**why**：直接在 Bash 工具里调 CLion 自带的 MinGW g++（不经这个脚本设 PATH）会失败，`cc1plus` 起不来。
这个脚本是用户提供的，专门用来让 AI 能在 Bash 工具里自己完成"改代码 → 构建 → 跑测试"的完整闭环，不用
每次都让用户手动到 CLion 里点构建。

**how to apply**：改完 `parser/`/`lexer/`（或任何会影响这两个测试目标的代码）之后，自己跑这个脚本
验证，不要让用户代跑。构建失败会跳过测试、返回非零；输出里会分别报 `SL_Cpp_Numeric_BigInt_Tests`、
`SL_Cpp_Numeric_BigDec_Tests`、`SL_Cpp_Lexer_Tests`、`SL_Cpp_Parser_Tests`、`SL_Cpp_Analyzer_Tests`
五个独立可执行文件各自的用例数/断言数。如果构建失败但报错信息在 Bash 输出里
被截断/看着不像真实原因，大概率是构建目录里的过时缓存状态，重新跑一次这个脚本（不用手动清理
`cmake-build-debug`）通常就能看到真实报错或者直接跑通——这个脚本本身跑得很快，多跑一次成本很低。
