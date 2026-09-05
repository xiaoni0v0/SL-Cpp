# 怎么自己构建、跑测试

`.ai/run_test.py`（脚本本身也在项目里，随文件夹走：项目根按脚本自己的位置推导，换台机器不用改）：
用 clang-cl + Ninja 配置 `cmake-build-debug` 的 Debug 构建，构建 all 目标后跑 ctest。三个阶段
（同步项目 / 构建 / 测试）任一失败就停在那里，并以那个阶段的退出码退出。

**怎么调用**：在 Bash 工具里执行

```
python E:\Programs\SL-Cpp\.ai\run_test.py
```

**why**：让 AI 能在 Bash 工具里自己完成"改代码 → 构建 → 跑测试"的完整闭环，不用每次都让用户手动到
CLion 里点构建。脚本写成 Python 而不是 `.bat`，是因为仓库不再存批处理脚本，见
[line-endings.md](line-endings.md)。

**how to apply**：改完 `compiler/` 下的代码（或任何会影响这几个测试目标的代码）之后，自己跑这个脚本
验证，不要让用户代跑。构建失败会跳过测试、返回非零；输出里会分别报 `SL_Cpp_Numeric_BigInt_Tests`、
`SL_Cpp_Numeric_BigDec_Tests`、`SL_Cpp_Lexer_Tests`、`SL_Cpp_Parser_Tests`、
`SL_Cpp_Analyzer_Tests` 五个独立可执行文件各自的用例数/断言数。如果构建失败但报错信息在 Bash 输出里
被截断/看着不像真实原因，大概率是构建目录里的过时缓存状态，重新跑一次这个脚本（不用手动清理
`cmake-build-debug`）通常就能看到真实报错或者直接跑通——这个脚本本身跑得很快，多跑一次成本很低。

**有一类缓存问题重跑解决不了**：报错形如 `error: Assume extern C functions don't unwind was
disabled in precompiled file '...cmake_pch.hxx.pch' but is currently enabled`，说明预编译头是用
另一套编译选项生成的，而 ninja 认为 `.pch` 已经是最新的、不会去重新生成它，所以跑多少次都是同一个
错。删掉 `.pch` 让它重建即可，不用清整个构建目录：

```
find cmake-build-debug -name "*.pch" -delete
```

（大批源文件的时间戳被一次性改动过之后容易触发，比如全仓库跑了一遍格式化。）
