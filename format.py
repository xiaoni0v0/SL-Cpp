#!/usr/bin/env python3

"""
format.py

批量格式化项目里的源文件，按文件名/后缀名分派给对应的格式化器（见 FORMATTERS_BY_NAME、FORMATTERS）：
    CMakeLists.txt                                  ->  gersemi
    .c / .cc / .cpp / .cxx / .h / .hh / .hpp / .hxx ->  clang-format
    .cmake                                          ->  gersemi
    .py                                             ->  black
未收录的文件名/后缀一律忽略。

排除规则由下面两张 pattern 表控制（glob 通配符，大小写不敏感）：
    EXCLUDE_DIR_PATTERNS    匹配目录名，命中的目录整棵子树都不进入
    EXCLUDE_FILE_PATTERNS   匹配文件名，命中的文件跳过

用法：
    python format.py [项目根目录] [-i]

参数：
    项目根目录       默认是当前目录 "."
    -i, --in-place   真正原地改写文件；不加这个参数只列出会被格式化的文件
"""

import argparse
import fnmatch
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Callable, NamedTuple
from typing import List

for _stream in sys.stdout, sys.stderr:
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(encoding="utf-8", errors="backslashreplace")

# 排除的目录名
EXCLUDE_DIR_PATTERNS = [
    ".ai",
    ".claude",
    ".git",
    ".idea",
    ".venv",
    "cmake-build-*",
]
# 排除的文件名
EXCLUDE_FILE_PATTERNS = []


class Formatter(NamedTuple):
    """一个格式化器：叫什么、怎么拼命令行、内容走 stdin 还是给路径"""

    command: str  # 可执行文件名，用于 PATH 检查和报错
    build_argv: Callable[[Path], List[str]]
    via_stdin: bool  # True 表示把原文件内容喂给 stdin

    def run(self, path: Path, original: bytes) -> bytes:
        """跑一遍，返回格式化后的内容；失败抛 FormatError"""
        result = subprocess.run(
            self.build_argv(path),
            input=original if self.via_stdin else b"",
            capture_output=True,
        )
        if result.returncode != 0:
            detail = result.stderr.decode("utf-8", errors="replace").strip()
            raise FormatError(detail or f"{self.command} 退出码 {result.returncode}")
        return result.stdout


class FormatError(RuntimeError):
    """格式化器处理单个文件失败"""


CLANG_FORMAT = Formatter(
    command="clang-format",
    build_argv=lambda path: ["clang-format", "--style=file", str(path)],
    via_stdin=False,
)
BLACK = Formatter(
    command="black",
    # black 只能从 stdin 读、往 stdout 写；--stdin-filename 让它找得到对应的 pyproject.toml
    build_argv=lambda path: ["black", "--quiet", "--stdin-filename", str(path), "-"],
    via_stdin=True,
)
GERSEMI = Formatter(
    command="gersemi",
    build_argv=lambda path: ["gersemi", "-"],
    via_stdin=True,
)

# 后缀名 -> 格式化器。要支持新语言就在这里加一行，别的地方不用动
FORMATTERS = {
    ".c": CLANG_FORMAT,
    ".cc": CLANG_FORMAT,
    ".cpp": CLANG_FORMAT,
    ".cxx": CLANG_FORMAT,
    ".h": CLANG_FORMAT,
    ".hh": CLANG_FORMAT,
    ".hpp": CLANG_FORMAT,
    ".hxx": CLANG_FORMAT,
    ".cmake": GERSEMI,
    ".py": BLACK,
}
# 按完整文件名分派的格式化器（大小写不敏感）
FORMATTERS_BY_NAME = {
    "cmakelists.txt": GERSEMI,
}


def matches_any(name: str, patterns: list[str]) -> bool:
    """大小写不敏感的通配符匹配，保证 Windows / Linux 上行为一致"""
    name = name.lower()
    return any(fnmatch.fnmatchcase(name, pattern.lower()) for pattern in patterns)


def formatter_for(path: Path) -> Formatter | None:
    """按完整文件名、再按后缀名分派格式化器，都没收录则返回 None"""
    return FORMATTERS_BY_NAME.get(path.name.lower()) or FORMATTERS.get(
        path.suffix.lower()
    )


def find_target_files(project_dir: Path) -> list[Path]:
    """查找所有待格式化的文件，跳过排除目录（整棵子树）和排除文件"""
    files = []
    for dirpath, dirnames, filenames in os.walk(project_dir):
        # 就地裁剪，别走进 build/ 这类可能有几十万文件的目录
        dirnames[:] = [d for d in dirnames if not matches_any(d, EXCLUDE_DIR_PATTERNS)]
        for name in filenames:
            if formatter_for(Path(name)) is None:
                continue
            if matches_any(name, EXCLUDE_FILE_PATTERNS):
                continue
            path = Path(dirpath) / name
            if path.is_file():  # 挡掉断链软链接、管道之类的非普通文件
                files.append(path)
    return sorted(files)


def format_file(path: Path) -> bool:
    """
    格式化单个文件。
    返回 True 表示内容确实发生了变化，False 表示格式化后与原内容一致（未改动）。
    """
    original = path.read_bytes()
    formatted = formatter_for(path).run(path, original)
    if formatted != original:
        path.write_bytes(formatted)
        return True
    return False


def pluralize(count: int, noun: str) -> str:
    return f"{count} {noun}" if count == 1 else f"{count} {noun}s"


def missing_commands(files: list[Path]) -> list[str]:
    """这批文件用得到、但 PATH 里找不到的格式化器"""
    needed = {formatter_for(f).command for f in files}
    return sorted(c for c in needed if shutil.which(c) is None)


def main():
    parser = argparse.ArgumentParser(description="批量格式化项目源文件")
    parser.add_argument(
        "project_dir", nargs="?", default=".", help="项目根目录，默认为当前目录"
    )
    parser.add_argument(
        "-i",
        "--in-place",
        action="store_true",
        help="真正原地改写文件；不加这个参数只列出会被格式化的文件",
    )
    args = parser.parse_args()

    project_dir = Path(args.project_dir).resolve()
    if not project_dir.is_dir():
        print(f"错误：目录不存在：{project_dir}", file=sys.stderr)
        sys.exit(1)

    files = find_target_files(project_dir)

    if not args.in_place:
        # 默认模式：只输出命中的文件名，不输出别的任何东西，方便管道接别的命令
        for f in files:
            print(f)
        return

    if missing := missing_commands(files):
        print(
            f"错误：找不到 {'、'.join(missing)}，请先确认已安装并在 PATH 中。",
            file=sys.stderr,
        )
        sys.exit(1)

    reformatted_count = unchanged_count = failed_count = 0

    for f in files:
        try:
            changed = format_file(f)
        except (FormatError, OSError) as e:
            # 单个文件失败不中断整批，最后统一汇总并以非零码退出
            print(f"错误：格式化失败 {f}：{e}", file=sys.stderr)
            failed_count += 1
            continue
        if changed:
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
    if failed_count > 0:
        summary_parts.append(pluralize(failed_count, "file") + " failed to reformat")
    print((", ".join(summary_parts) + ".") if summary_parts else "No files matched.")

    if failed_count > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
