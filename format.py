#!/usr/bin/env python3
"""
format_project.py
批量用 clang-format 格式化项目里的 .h / .cpp 文件
排除规则：文件名以 "x_" 开头、以 ".h" 结尾的内联 X-Macro 头文件（如 x_token_type.h）

用法：
    python format_project.py [项目根目录] [--dry-run]

参数：
    项目根目录   默认是当前目录 "."
    --dry-run    只打印将要格式化的文件列表，不实际执行 clang-format
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# 在环境变量里加入
os.environ["PATH"] = (
    r"C:\Program Files\JetBrains\CLion 2026.1\plugins\clion-radler\DotFiles\windows-x64;"
    r"C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin;"
    r"C:\Program Files\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin;"
    r"C:\Program Files\JetBrains\CLion 2026.1\bin\ninja;" + os.environ["PATH"]
)

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


def main():
    parser = argparse.ArgumentParser(description="批量用 clang-format 格式化项目文件")
    parser.add_argument(
        "project_dir", nargs="?", default=".", help="项目根目录，默认为当前目录"
    )
    parser.add_argument(
        "--dry-run", action="store_true", help="只打印文件列表，不实际格式化"
    )
    args = parser.parse_args()

    project_dir = Path(args.project_dir).resolve()
    if not project_dir.is_dir():
        print(f"错误：目录不存在：{project_dir}", file=sys.stderr)
        sys.exit(1)

    files = find_target_files(project_dir)
    print(f"共找到 {len(files)} 个待格式化文件（已排除 x_*.h）。")

    if args.dry_run:
        for f in files:
            print(f)
        return

    if shutil.which("clang-format") is None:
        print(
            "错误：找不到 clang-format，请先确认已安装并在 PATH 中。", file=sys.stderr
        )
        sys.exit(1)

    for f in files:
        print(f"格式化: {f}")
        subprocess.run(["clang-format", "-i", "--style=file", str(f)], check=True)

    print("完成。")


if __name__ == "__main__":
    main()
