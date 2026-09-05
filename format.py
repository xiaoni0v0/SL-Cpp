#!/usr/bin/env python3

"""
format.py

批量格式化项目里的源文件，按文件名/后缀名分派给对应的格式化器（见 FORMATTERS_BY_NAME、FORMATTERS）：
    .cpp / .h / .inc                           ->  clang-format
    CMakeLists.txt                             ->  gersemi
    .cmake                                     ->  gersemi
    .py                                        ->  black
    .md / .json / .yaml / .yml / .clang-format ->  prettier
未收录的文件名/后缀一律忽略。

排除规则由下面两张 pattern 表控制（glob 通配符，大小写不敏感）：
    EXCLUDE_DIR_PATTERNS    匹配目录名，命中的目录整棵子树都不进入
    EXCLUDE_FILE_PATTERNS   匹配文件名，命中的文件跳过

编码一律 UTF-8：本脚本不猜编码、也不代为转换，遇到不是合法 UTF-8 的文件只报错并跳过，要转码得由你自己来；
带 UTF-8 BOM 的则会在写回时顺手把 BOM 剥掉。

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
import time
from pathlib import Path
from typing import Callable, NamedTuple
from typing import List

for _stream in sys.stdout, sys.stderr:
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(
            encoding="utf-8",
            errors="backslashreplace",
            # Windows 上 print 默认把 \n 翻成 \r\n，下游 read/xargs 会收到末尾带 \r 的路径
            newline="\n",
            # 不设的话 stdout 在重定向时是块缓冲，stderr 却是即时的，两股输出的先后会整个错位
            line_buffering=True,
        )

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
EXCLUDE_FILE_PATTERNS = [
    "big_int_cases.inc",  # test/numeric/gen_big_int_cases.py 生成
    "big_dec_cases.inc",  # test/numeric/gen_big_dec_cases.py 生成
]
# 单个格式化器进程的超时（秒），防止某个工具挂死后整批无声无息地卡住
TIMEOUT_SECONDS = 60
# UTF-8 BOM。项目统一 UTF-8 无 BOM，读进来遇到就剥掉
UTF8_BOM = b"\xef\xbb\xbf"


class Formatter(NamedTuple):
    """一个格式化器：叫什么、怎么拼命令行、内容走 stdin 还是给路径"""

    command: str  # 可执行文件名，用于 PATH 检查和报错
    build_argv: Callable[[Path], List[str]]
    via_stdin: bool  # True 表示把原文件内容喂给 stdin
    install_hint: str  # 没装时告诉用户怎么装

    def run(self, path: Path, original: bytes) -> bytes:
        """跑一遍，返回格式化后的内容；失败抛 FormatError"""
        argv = self.build_argv(path)
        argv[0] = shutil.which(self.command) or argv[0]
        try:
            result = subprocess.run(
                argv,
                input=original if self.via_stdin else b"",
                capture_output=True,
                timeout=TIMEOUT_SECONDS,
            )
        except subprocess.TimeoutExpired:
            # TimeoutExpired 不是 OSError，不转成 FormatError 的话 main 里拦不住
            raise FormatError(
                f"{self.command} 超过 {TIMEOUT_SECONDS} 秒还没返回，已放弃这个文件"
            ) from None
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        if result.returncode != 0:
            raise FormatError(detail or f"{self.command} 退出码 {result.returncode}")
        if original and not result.stdout:
            # 退出码 0 但什么都没输出，照写回去等于把源文件清空
            raise FormatError(detail or f"{self.command} 退出码 0 却没有任何输出")
        if detail:
            # 成功时的告警也得让人看见，别被 capture_output 吞了
            report_progress(
                f"警告：{self.command} {path}：{indent_detail(detail)}", to_stderr=True
            )
        return result.stdout


class FormatError(RuntimeError):
    """格式化器处理单个文件失败"""


# 循环里是否已经打过逐文件的信息（reformatted 行、告警、报错），决定最后要不要空一行把它们和总结隔开
_progress_printed = False


def report_progress(message: str, *, to_stderr: bool = False) -> None:
    """打一行逐文件的进度/告警/报错"""
    global _progress_printed
    _progress_printed = True
    print(message, file=sys.stderr if to_stderr else sys.stdout)


def indent_detail(detail: str) -> str:
    """
    工具吐出来的多行信息缩进成一整块。
    不缩的话续行顶格顶在最左边，看着像另一条独立消息，跟前后的输出糊成一片。
    """
    lines = detail.splitlines()
    if len(lines) <= 1:
        return detail
    return "\n" + "\n".join(f"    {line}" if line else "" for line in lines)


CLANG_FORMAT = Formatter(
    command="clang-format",
    build_argv=lambda path: ["clang-format", "--style=file", str(path)],
    via_stdin=False,
    install_hint="pip install clang-format",
)
BLACK = Formatter(
    command="black",
    build_argv=lambda path: ["black", "--quiet", "--stdin-filename", str(path), "-"],
    via_stdin=True,
    install_hint="pip install black",
)
GERSEMI = Formatter(
    command="gersemi",
    build_argv=lambda path: ["gersemi", str(path)],
    via_stdin=False,
    install_hint="pip install gersemi",
)
PRETTIER = Formatter(
    command="prettier",
    build_argv=lambda path: [
        "prettier",
        "--prose-wrap",
        "preserve",
        "--stdin-filepath",
        str(path),
    ],
    via_stdin=True,
    install_hint="npm install -g prettier",
)

# 后缀名 -> 格式化器。要支持新语言就在这里加一行，别的地方不用动
FORMATTERS = {
    ".cpp": CLANG_FORMAT,
    ".h": CLANG_FORMAT,
    ".inc": CLANG_FORMAT,
    ".cmake": GERSEMI,
    ".py": BLACK,
    ".md": PRETTIER,
    ".json": PRETTIER,
    ".yaml": PRETTIER,
    ".yml": PRETTIER,
}
# 按完整文件名分派的格式化器（大小写不敏感）
FORMATTERS_BY_NAME = {
    "cmakelists.txt": GERSEMI,
    ".clang-format": PRETTIER,
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


def ensure_utf8(original: bytes) -> None:
    """
    编码闸门：不是合法 UTF-8 就抛 FormatError，让这个文件整个跳过。

    这里绝不猜编码、绝不代为转换——猜错一次就是不可逆的内容损坏。拦在跑格式化器之前，是因为
    工具们对非 UTF-8 的反应并不一致：black 和 gersemi 会拒绝（退出码非零，拦得住），但 prettier
    会把非法字节按 UTF-8 解成一串 U+FFFD 替换字符、再编码输出，而且退出码是 0——照写回去，原文
    就永久没了。
    """
    try:
        original.decode("utf-8")
    except UnicodeDecodeError as e:
        raise FormatError(
            f"不是合法的 UTF-8（第 {e.start} 字节起：{e.reason}），已跳过不动；"
            f"请你把它转成 UTF-8 之后重新运行本脚本"
        ) from None


def format_file(path: Path) -> bool:
    """
    格式化单个文件。
    返回 True 表示内容确实发生了变化，False 表示格式化后与原内容一致（未改动）。
    """
    original = path.read_bytes()
    ensure_utf8(original)
    # 四个格式化器都是原样保留 BOM 的，所以进出各剥一次
    source = original.removeprefix(UTF8_BOM)
    formatted = formatter_for(path).run(path, source).removeprefix(UTF8_BOM)
    if formatted != original:
        path.write_bytes(formatted)
        return True
    return False


def pluralize(count: int, noun: str) -> str:
    return f"{count} {noun}" if count == 1 else f"{count} {noun}s"


def missing_formatters(files: list[Path]) -> list[Formatter]:
    """这批文件用得到、但 PATH 里找不到的格式化器"""
    needed = {formatter_for(f).command: formatter_for(f) for f in files}
    return [fm for _, fm in sorted(needed.items()) if shutil.which(fm.command) is None]


def main() -> None:
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

    started_at = time.perf_counter()
    try:
        run(args)
    finally:
        print(f"耗时 {time.perf_counter() - started_at:.2f} 秒", file=sys.stderr)


def run(args: argparse.Namespace) -> None:
    """参数已经解析好之后的正事"""
    project_dir = Path(args.project_dir).resolve()
    if not project_dir.is_dir():
        print(f"错误：目录不存在：{project_dir}", file=sys.stderr)
        sys.exit(1)

    files = find_target_files(project_dir)

    global _progress_printed
    _progress_printed = False  # 被当模块 import 反复调用时也从干净状态起步

    if not args.in_place:
        # 默认模式：只输出命中的文件名，不输出别的任何东西，方便管道接别的命令
        for f in files:
            print(f)
        return

    if missing := missing_formatters(files):
        print(
            "错误：下面这些格式化器不在 PATH 里，一个文件都没处理。"
            "请你按右边的命令装好，然后重新运行本脚本：",
            file=sys.stderr,
        )
        for fm in missing:
            print(f"    {fm.command:<14}{fm.install_hint}", file=sys.stderr)
        sys.exit(1)

    reformatted_count = unchanged_count = failed_count = 0

    for f in files:
        try:
            changed = format_file(f)
        except (FormatError, OSError) as e:
            # 单个文件失败不中断整批，最后统一汇总并以非零码退出
            report_progress(
                f"错误：格式化失败 {f}：{indent_detail(str(e))}", to_stderr=True
            )
            failed_count += 1
            continue
        if changed:
            report_progress(f"reformatted {f}")
            reformatted_count += 1
        else:
            unchanged_count += 1

    if _progress_printed:
        print()  # 把上面那堆逐文件的输出和总结行隔开

    print("Oh no! 💥 💔 💥" if failed_count > 0 else "All done! ✨ 🍰 ✨")

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
