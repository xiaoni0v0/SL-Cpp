#!/usr/bin/env python3

"""
format.py
批量用 clang-format 格式化项目里的 .h / .cpp 文件
排除规则：文件名以 "x_" 开头、以 ".h" 结尾的内联 X-Macro 头文件（如 x_token_type.h）

用法：
    python format.py [项目根目录] [--dry-run]

参数：
    项目根目录   默认是当前目录 "."
    --dry-run    只打印命中的文件名列表，不实际执行 clang-format
"""

import argparse
import subprocess
import sys
from pathlib import Path

# clang-format 可执行文件路径
CLANG_FORMAT_EXE = r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\Llvm\x64\bin\clang-format.exe"

# 默认额外排除的目录（噪音目录，可按需增删）
EXCLUDE_DIRS = {".git", "build"}
# 支持通配符的排除目录（比如 cmake-build-debug、cmake-build-release）
EXCLUDE_DIR_PATTERNS = ["cmake-build-*"]


def is_excluded_dir(path: Path) -> bool:
    """判断路径中是否包含需要排除的目录"""
    for part in path.parts:
        if part in EXCLUDE_DIRS:
            return True
        for pattern in EXCLUDE_DIR_PATTERNS:
            if Path(part).match(pattern):
                return True
    return False


def find_target_files(project_dir: Path) -> list[Path]:
    """查找所有待格式化的 .h / .cpp 文件，排除噪音目录和 x_*.h 内联头文件"""
    files = []
    for ext in ("*.h", "*.cpp"):
        for path in project_dir.rglob(ext):
            if is_excluded_dir(path.relative_to(project_dir).parent):
                continue
            if path.name.startswith("x_") and path.suffix == ".h":
                continue
            files.append(path)
    return sorted(files)


def format_file(path: Path) -> bool:
    """
    用 clang-format 格式化单个文件。
    返回 True 表示内容确实发生了变化，False 表示格式化后与原内容一致（未改动）。
    """
    original = path.read_bytes()
    result = subprocess.run(
        [CLANG_FORMAT_EXE, "--style=file", str(path)],
        capture_output=True,
        check=True,
    )
    formatted = result.stdout
    if formatted != original:
        path.write_bytes(formatted)
        return True
    return False


def pluralize(count: int, noun: str) -> str:
    """按 black 的习惯做单复数：1 file / 2 files"""
    return f"{count} {noun}" if count == 1 else f"{count} {noun}s"


def main():
    parser = argparse.ArgumentParser(description="批量用 clang-format 格式化项目文件")
    parser.add_argument(
        "project_dir", nargs="?", default=".", help="项目根目录，默认为当前目录"
    )
    parser.add_argument(
        "--dry-run", action="store_true", help="只打印命中的文件名，不实际格式化"
    )
    args = parser.parse_args()

    project_dir = Path(args.project_dir).resolve()
    if not project_dir.is_dir():
        print(f"错误：目录不存在：{project_dir}", file=sys.stderr)
        sys.exit(1)

    files = find_target_files(project_dir)

    if args.dry_run:
        # dry-run 模式：只输出命中的文件名，不输出别的任何东西
        for f in files:
            print(f)
        return

    if not Path(CLANG_FORMAT_EXE).is_file():
        print(f"错误：找不到 clang-format：{CLANG_FORMAT_EXE}", file=sys.stderr)
        print("请修改脚本里的 CLANG_FORMAT_EXE 常量为正确路径。", file=sys.stderr)
        sys.exit(1)

    reformatted_count = 0
    unchanged_count = 0

    for f in files:
        if format_file(f):
            print(f"reformatted {f}")
            reformatted_count += 1
        else:
            unchanged_count += 1

    if reformatted_count > 0:
        print()  # 分隔 reformatted 列表和总结行

    print("All done! ✨ 🍰 ✨")

    summary_parts = []
    if reformatted_count > 0:
        summary_parts.append(pluralize(reformatted_count, "file") + " reformatted")
    if unchanged_count > 0:
        summary_parts.append(pluralize(unchanged_count, "file") + " left unchanged")
    print(", ".join(summary_parts) + ".")


if __name__ == "__main__":
    main()
