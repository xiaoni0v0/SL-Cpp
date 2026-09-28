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
验证，不要让用户代跑。构建失败会跳过测试、返回非零；输出里会分别报 `sl_test_numeric_bigint`、
`sl_test_numeric_bigdec`、`sl_test_lexer`、`sl_test_parser`、
`sl_test_analyzer` 五个独立可执行文件各自的用例数/断言数。如果构建失败但报错信息在 Bash 输出里
被截断/看着不像真实原因，大概率是构建目录里的过时缓存状态，重新跑一次这个脚本（不用手动清理
`cmake-build-debug`）通常就能看到真实报错或者直接跑通——这个脚本本身跑得很快，多跑一次成本很低。

**有一类缓存问题重跑解决不了**：报错形如 `error: Assume extern C functions don't unwind was
disabled in precompiled file '...cmake_pch.hxx.pch' but is currently enabled`，说明预编译头是用
另一套编译选项生成的，而 ninja 认为 `.pch` 已经是最新的、不会去重新生成它，所以跑多少次都是同一个
错。

**根因是编译器混用**，不是构建目录脏了：这个脚本用的是 clang-cl，而 CLion 的工具链里如果配的是
clang，两边共用同一个 `cmake-build-debug`，谁编译谁就把对方的 `.pch` 顶掉。**开发期间统一用
clang-cl**（CLion 里也选 clang-cl）之后这个错不再出现。

**how to apply**：见到这个错，先确认两边用的是不是同一个编译器，而不是去删 `.pch`——更不要删整个
`cmake-build-debug`。删掉只是绕过症状，下次从另一边编译又会犯，而且重建构建目录很慢。

## Release 发布构建，以及 SL_NATIVE 开关

`run_test.py` 只跑 Debug。要构建发布版，配置时把 `-DCMAKE_BUILD_TYPE=Debug` 换成 `Release`
（其余参数不变，`-DCMAKE_MAKE_PROGRAM=ninja` 那一套照抄）：

```
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -G Ninja -S . -B cmake-build-release
cmake --build cmake-build-release --target all -j 22
```

- **Release**：只以运行速度为目标（不管编译时间/产物体积），但不加任何会改变浮点运算结果的参数
  （没有 `-ffast-math` 一类），也不用目标 CPU 专属指令集，编译产物能直接拿到别的机器上跑。
- **`-DSL_NATIVE=ON`**：在 Release 基础上再加 `-march=native`/`-mtune=native`（GCC/Clang/clang-cl；
  真正的 cl.exe 没有这个能力，未知选项直接忽略，警告 D9002，没有"探测本机 CPU"这回事，`/arch:AVX2`
  那一套是固定枚举不是自动探测，所以没伪造一个假的），只保证在编译用的这台机器上能跑、可能比普通
  Release 更快，编译产物不能挪到别的机器上：

  ```
  cmake -DCMAKE_BUILD_TYPE=Release -DSL_NATIVE=ON -DCMAKE_MAKE_PROGRAM=ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -G Ninja -S . -B cmake-build-native
  ```

  **没做成单独一档 CMAKE_BUILD_TYPE**（比如叫 "Native"）：CLion 的 CMake Profile 设置里"Build
  type"是个只认 `Debug`/`Release`/`RelWithDebInfo`/`MinSizeRel` 的下拉框，自定义名字输不进去（哪怕
  命令行 `-DCMAKE_BUILD_TYPE=Native` 本身完全合法）。做成 `option()` 开关就没有这个限制：CLion 里在
  对应 Profile 的 "CMake options" 文本框里加 `-DSL_NATIVE=ON`，或者首次配置完之后在同一页面的 Cache
  变量列表里勾选 `SL_NATIVE`，都不经过那个下拉框。
  - Release 因为是 CMake 内置档，`NDEBUG`/LTO 这些都是自带的；`SL_NATIVE` 只是在同一个 Release 档
    上追加 `-march=native`/`-mtune=native` 两个参数，不引入新的 CMAKE_BUILD_TYPE 取值，所以不用
    像自定义档那样把每个参数手动补全。
- 四种编译器身份（GCC、Clang、clang-cl、真 cl.exe）的 Release、`SL_NATIVE=ON` 都跑过真实构建 + 全部
  测试验证过，行为跟 Debug 一致（差分测试的数值结果不受影响）。

## 用 ASan + UBSan 跑全部测试（验证有没有 UB）

`run_test.py` 用的 Debug 构建不带 sanitizer。要真跑 UB 检测，在**临时目录**里另建一个构建（别动 `cmake-build-debug`）：

- **ASan 不支持调试版 CRT（`-MDd`），而项目 Debug 强制用它**：把项目源码拷一份到临时目录，只在拷贝的
  `CMakeLists.txt` 里把 `MultiThreadedDebugDLL`/`MultiThreaded` 都改成 `MultiThreadedDLL`，再用
  `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL`。
- 编译参数 `-fsanitize=address,undefined -fno-sanitize-recover=undefined`（UB 一触发就终止，测试直接失败）；
  `CMAKE_CXX_FLAGS_DEBUG` 里去掉 `/RTC1`。
- **CMake 是直接调 `lld-link` 的，不经过 clang-cl 驱动，所以 sanitizer 运行库不会自动链进来**：要给
  `CMAKE_EXE_LINKER_FLAGS` 手动加 `-libpath:<LLVM>/lib/clang/22/lib/windows` 和
  `clang_rt.asan_dynamic-x86_64.lib clang_rt.asan_dynamic_runtime_thunk-x86_64.lib
  clang_rt.ubsan_standalone-x86_64.lib clang_rt.ubsan_standalone_cxx-x86_64.lib`（路径有空格用 8.3 短路径）。
- 跑测试前把 `<LLVM>/lib/clang/22/lib/windows` 放进 `PATH`（要 `clang_rt.asan_dynamic-x86_64.dll`），
  `ASAN_OPTIONS=detect_leaks=0`（Windows 没有 LeakSanitizer）。
- 在 Git Bash 里传 `/Zi` 这类 MSVC 风格参数会被当成路径改写，一律写成 `-Zi`。
- 全部测试约 2 分钟（BigDec 的差分测试占大头），比普通构建慢 4 倍左右。

