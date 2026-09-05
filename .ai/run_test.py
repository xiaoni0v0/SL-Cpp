#!/usr/bin/env python3

"""
run_test.py

构建并跑测试的一条龙脚本：配置 CMake（Ninja + clang-cl，Debug）-> 构建 all 目标 -> ctest。
任一阶段失败就停在那里，并以那个阶段的退出码退出。

用法：
    python .ai/run_test.py
"""

import ctypes
import os
import subprocess
import sys
from pathlib import Path

# 脚本在 .ai/ 下，项目根就是它的上一级；整个文件夹搬到别处也不用改路径
PROJECT_DIR = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_DIR / "cmake-build-debug"
JOBS = "22"

CONFIGURE_ARGV = [
    "cmake",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DCMAKE_MAKE_PROGRAM=ninja",
    "-DCMAKE_C_COMPILER=clang-cl",
    "-DCMAKE_CXX_COMPILER=clang-cl",
    "-G",
    "Ninja",
    "-S",
    str(PROJECT_DIR),
    "-B",
    str(BUILD_DIR),
]
BUILD_ARGV = ["cmake", "--build", str(BUILD_DIR), "--target", "all", "-j", JOBS]
TEST_ARGV = ["ctest", "--extra-verbose", "-j", JOBS]

# 每个阶段：标题、命令、工作目录、失败时补印的那句话
# 最后一个阶段（测试）失败不补话，直接把 ctest 的退出码透传出去
STAGES = [
    (
        "开始同步项目",
        CONFIGURE_ARGV,
        PROJECT_DIR,
        "同步项目失败，已跳过构建和测试",
    ),
    ("开始构建", BUILD_ARGV, PROJECT_DIR, "构建失败，已跳过测试"),
    ("开始测试", TEST_ARGV, BUILD_DIR, None),
]


def main() -> int:
    if os.name == "nt":
        # 对应原来的 chcp 65001：控制台代码页切到 UTF-8，子进程的中文输出才不乱码
        ctypes.windll.kernel32.SetConsoleOutputCP(65001)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")

    for index, (title, argv, cwd, failure) in enumerate(STAGES):
        if index > 0:
            print()
        print(f"===== {title} =====")
        print()
        # 子进程是直接往 fd 写的，自己的输出得先冲出去，不然顺序会乱
        sys.stdout.flush()
        try:
            code = subprocess.run(argv, cwd=cwd).returncode
        except OSError as e:
            print(f"错误：无法执行 {argv[0]}：{e}", file=sys.stderr)
            code = 9009  # 跟 cmd.exe 找不到命令时的 errorlevel 保持一致
        if code != 0:
            if failure is not None:
                print()
                print(f"===== {failure} =====")
            return code
    return 0


if __name__ == "__main__":
    sys.exit(main())
