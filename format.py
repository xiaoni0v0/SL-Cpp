#!/usr/bin/env python3

"""
format.py

批量格式化项目源文件，按完整文件名、其次按后缀分派工具：

    .cpp / .h / .inc                            ->  clang-format
    CMakeLists.txt / .cmake                     ->  gersemi
    .py                                         ->  black
    .md / .json / .yaml / .yml / .clang-format  ->  prettier
    其余忽略

命中 EXCLUDE_DIR_PATTERNS（整棵子树）或 EXCLUDE_FILE_PATTERNS 的文件不处理。
按 UTF-8 处理：非法编码报错跳过（不猜测、不转码），写回时剥 BOM。

-i 与 -c 模式把“盘上内容等于格式化结果”的文件的（修改时间, 大小）记入
项目根目录的 CACHE_FILENAME（每项目一份，互不干扰），命中即跳过，省得下次再跑
格式化器探测；脚本自身、格式化器或配置变化会使缓存整体失效（见
environment_fingerprint）。-c 只缓存确认为干净的已扫描文件，不把待改文件
当作已格式化记进去。

用法：python format.py [项目根目录] [-i | -c] [--no-cache]
    项目根目录   默认当前目录
    缺省          仅列出本脚本“会管到”的候选文件（不跑格式化器）
    -i           原地改写内容有变化的文件
    -c           打印所有会被真正改写（内容将变化）的文件，不写盘；
                 有待改文件或出错时退出码非零，可作 CI 门禁
    --no-cache   忽略缓存强制重跑（仅 -i / -c 有效）
"""

import argparse
import fnmatch
import functools
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable, NamedTuple

# Windows 下 print 输出 \r\n、重定向时又块缓冲，会污染管道并乱序，统一修正
for _stream in sys.stdout, sys.stderr:
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(
            encoding="utf-8",
            errors="backslashreplace",
            newline="\n",
            line_buffering=True,
        )

# 缓存放被格式化的项目根目录，各项目互不干扰；结构变化时递增版本号作废旧缓存
CACHE_FILENAME = ".format_cache.json"
CACHE_VERSION = 1

# 命中则整棵子树跳过（glob，忽略大小写）
EXCLUDE_DIR_PATTERNS = [
    ".ai",
    ".claude",
    ".git",
    ".idea",
    ".venv",
    "cmake-build-*",
]
# 命中则跳过
EXCLUDE_FILE_PATTERNS = [
    # 由 test/numeric/gen_*_cases.py 生成
    "big_int_cases.inc",
    "big_dec_cases.inc",
    # 缓存文件，每次运行整体重写
    CACHE_FILENAME,
]
# 单次格式化超时（秒），防工具挂起拖住整批
TIMEOUT_SECONDS = 60
# UTF-8 BOM，读入时剥除
UTF8_BOM = b"\xef\xbb\xbf"
# 影响格式结果的配置文件，纳入指纹
CONFIG_FILENAMES = [".clang-format", ".gersemirc"]


@functools.lru_cache(maxsize=None)
def which_cached(command: str) -> str | None:
    """缓存 shutil.which 的结果，避免逐文件扫描 PATH"""
    return shutil.which(command)


class Formatter(NamedTuple):
    """格式化器：命令名、命令行构造、内容传递方式与安装提示"""

    command: str  # 可执行文件名，用于 PATH 检查与报错
    build_argv: Callable[[Path], list[str]]
    via_stdin: bool  # True 时把原文件内容经 stdin 传入
    install_hint: str  # 未安装时的安装命令

    def run(self, path: Path, original: bytes) -> bytes:
        """执行格式化，返回结果；失败抛 FormatError"""
        argv = self.build_argv(path)
        argv[0] = which_cached(self.command) or argv[0]
        try:
            result = subprocess.run(
                argv,
                input=original if self.via_stdin else b"",
                capture_output=True,
                timeout=TIMEOUT_SECONDS,
            )
        except subprocess.TimeoutExpired:
            # TimeoutExpired 非 OSError，转 FormatError 以统一捕获
            raise FormatError(
                f"{self.command} 超过 {TIMEOUT_SECONDS} 秒未返回，放弃该文件"
            ) from None
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        if result.returncode != 0:
            raise FormatError(detail or f"{self.command} 退出码 {result.returncode}")
        if original and not result.stdout:
            # 退出码 0 却无输出，直接写回会清空文件
            raise FormatError(detail or f"{self.command} 退出码 0 但无输出")
        if detail:
            # 成功时的 stderr 告警也要展示
            report_progress(
                f"警告：{self.command}（{path}）：{indent_detail(detail)}",
                to_stderr=True,
            )
        return result.stdout


class FormatError(RuntimeError):
    """单个文件格式化失败"""


# 是否已输出逐文件消息，决定结尾是否补空行隔开总结
_progress_printed = False


def report_progress(message: str, *, to_stderr: bool = False) -> None:
    """输出一行逐文件消息"""
    global _progress_printed
    _progress_printed = True
    print(message, file=sys.stderr if to_stderr else sys.stdout)


def indent_detail(detail: str) -> str:
    """多行信息整体缩进，避免与其它输出混淆"""
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

# 后缀 -> 格式化器；支持新语言在此追加
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
# 完整文件名 -> 格式化器（大小写不敏感）
FORMATTERS_BY_NAME = {
    "cmakelists.txt": GERSEMI,
    ".clang-format": PRETTIER,
}


def matches_any(name: str, patterns: list[str]) -> bool:
    """忽略大小写的通配匹配，跨平台行为一致"""
    name = name.lower()
    return any(fnmatch.fnmatchcase(name, pattern.lower()) for pattern in patterns)


def formatter_for(path: Path) -> Formatter | None:
    """按完整文件名、再按后缀名查找格式化器；未收录返回 None"""
    # NamedTuple 的真值不表示“有无”，须显式与 None 比较
    by_name = FORMATTERS_BY_NAME.get(path.name.lower())
    if by_name is not None:
        return by_name
    return FORMATTERS.get(path.suffix.lower())


def find_target_files(project_dir: Path) -> list[Path]:
    """列出待格式化文件，跳过排除目录（整棵子树）与排除文件"""
    files = []
    for dirpath, dirnames, filenames in os.walk(project_dir):
        dirnames[:] = [d for d in dirnames if not matches_any(d, EXCLUDE_DIR_PATTERNS)]
        for name in filenames:
            if formatter_for(Path(name)) is None:
                continue
            if matches_any(name, EXCLUDE_FILE_PATTERNS):
                continue
            path = Path(dirpath) / name
            if path.is_file():  # 排除断链软链接、管道等非普通文件
                files.append(path)
    return sorted(files)


def ensure_utf8(original: bytes) -> None:
    """非合法 UTF-8 抛 FormatError（只拦不转码）。

    prettier 会把坏字节解成 U+FFFD 且退出码为 0，直接写回会损坏原文，
    故须在调格式化器前检查。"""
    try:
        original.decode("utf-8")
    except UnicodeDecodeError as e:
        raise FormatError(
            f"非法 UTF-8（第 {e.start} 字节起：{e.reason}），跳过；请先转码"
        ) from None


def write_atomic(path: Path, data: bytes) -> None:
    """写同目录临时文件后原子替换，防中断留残缺；跨盘替换非原子，临时文件须同目录"""
    temporary = path.with_name(path.name + ".format-tmp")
    try:
        temporary.write_bytes(data)
        os.replace(temporary, path)
    except OSError:
        temporary.unlink(missing_ok=True)
        raise


def format_file(path: Path, *, write: bool = True) -> bool:
    """格式化单个文件并返回内容是否有变化。

    有变化且 write 为 True 时原子写回；write=False（-c 用）只探测不改盘。"""
    original = path.read_bytes()
    ensure_utf8(original)
    # 格式化器会原样保留 BOM，进出各剥一次
    source = original.removeprefix(UTF8_BOM)
    formatted = formatter_for(path).run(path, source).removeprefix(UTF8_BOM)
    if formatted == original:
        return False
    if write:
        write_atomic(path, formatted)
    return True


def needed_formatters(files: list[Path]) -> list[Formatter]:
    """本批所需格式化器，按命令名排序（保证指纹稳定）"""
    by_command = {formatter_for(f).command: formatter_for(f) for f in files}
    # 按命令名排序查表，避免比较含 lambda 的 Formatter
    return [by_command[command] for command in sorted(by_command)]


def missing_formatters(files: list[Path]) -> list[Formatter]:
    """所需但不在 PATH 的格式化器"""
    return [fm for fm in needed_formatters(files) if which_cached(fm.command) is None]


def stat_signature(path: Path | None) -> list[int] | None:
    """返回（修改时间, 大小）；路径无效时为 None"""
    if path is None:
        return None
    try:
        info = path.stat()
    except OSError:
        return None
    return [info.st_mtime_ns, info.st_size]


def environment_fingerprint(project_dir: Path, formatters: list[Formatter]) -> str:
    """外部因素指纹：脚本自身、格式化器可执行文件、配置文件，任一变化即缓存失效。

    用（路径, 修改时间, 大小）代替 --version 查询；升级未替换可执行文件时
    指纹察觉不到，可用 --no-cache 强制重跑。"""
    parts = [f"v{CACHE_VERSION}", f"self={stat_signature(Path(__file__).resolve())}"]
    for fm in formatters:
        exe = which_cached(fm.command)
        signature = stat_signature(Path(exe)) if exe else None
        parts.append(f"{fm.command}={exe}@{signature}")
    for name in CONFIG_FILENAMES:
        parts.append(f"{name}@{stat_signature(project_dir / name)}")
    return "|".join(parts)


def load_cache(cache_path: Path, fingerprint: str) -> dict[str, list[int]]:
    """读缓存；缺失/损坏/指纹不符均视为无缓存"""
    try:
        with cache_path.open(encoding="utf-8") as fp:
            data = json.load(fp)
    except (OSError, ValueError):
        return {}
    if not isinstance(data, dict) or data.get("fingerprint") != fingerprint:
        return {}
    entries = data.get("files")
    return entries if isinstance(entries, dict) else {}


def save_cache(
    cache_path: Path, fingerprint: str, entries: dict[str, list[int]]
) -> None:
    """先写临时文件再替换，防留下半截 JSON"""
    temporary = cache_path.with_name(CACHE_FILENAME + ".tmp")
    try:
        with temporary.open("w", encoding="utf-8", newline="\n") as fp:
            json.dump(
                {"fingerprint": fingerprint, "files": entries},
                fp,
                ensure_ascii=False,
                indent=1,
                sort_keys=True,
            )
        os.replace(temporary, cache_path)
    except OSError as e:
        # 写缓存失败不影响结果，最多下次全量重跑
        print(
            f"警告：写缓存失败：{cache_path}（{e}），下次全量重跑",
            file=sys.stderr,
        )


def main() -> None:
    parser = argparse.ArgumentParser(description="批量格式化项目源文件")
    parser.add_argument(
        "project_dir", nargs="?", default=".", help="项目根目录（默认当前目录）"
    )
    parser.add_argument(
        "-i",
        "--inplace",
        action="store_true",
        help="原地改写内容有变化的文件",
    )
    parser.add_argument(
        "-c",
        "--check",
        action="store_true",
        help="只打印会被真正改写的文件（不写盘）；有待改文件时退出码为 1",
    )
    parser.add_argument(
        "--no-cache",
        action="store_true",
        help="忽略缓存强制重跑（仅 -i / -c 有效）",
    )
    args = parser.parse_args()
    if args.inplace and args.check:
        parser.error("-i 与 -c 互斥")
    if args.no_cache and not (args.inplace or args.check):
        # 缺省模式不读写缓存，此组合无任何效果
        parser.error("--no-cache 仅适用于 -i / -c")

    started_at = time.perf_counter()
    try:
        run(args)
    finally:
        print(f"耗时 {time.perf_counter() - started_at:.2f} 秒", file=sys.stderr)


def run(args: argparse.Namespace) -> None:
    """参数解析完成后的主流程"""
    project_dir = Path(args.project_dir).resolve()
    if not project_dir.is_dir():
        print(f"错误：目录不存在：{project_dir}", file=sys.stderr)
        sys.exit(1)

    files = find_target_files(project_dir)

    global _progress_printed
    _progress_printed = False  # 多次调用（含 import 复用）也从干净状态开始

    if not args.inplace and not args.check:
        # 缺省模式：stdout 只输出本脚本会处理的文件名，便于管道
        for f in files:
            print(f)
        return

    # -i 与 -c 都依赖格式化器与缓存，流程在此汇合；二者仅差在是否写盘、
    # 往 stdout 打什么、以及退出码。
    if missing := missing_formatters(files):
        print(
            "错误：以下格式化器不在 PATH，未处理任何文件。请安装后重试：",
            file=sys.stderr,
        )
        for fm in missing:
            print(f"    {fm.command:<14}{fm.install_hint}", file=sys.stderr)
        sys.exit(1)

    fingerprint = environment_fingerprint(project_dir, needed_formatters(files))
    # 缓存放被格式化目录下，各项目互不干扰
    cache_path = project_dir / CACHE_FILENAME
    cached = {} if args.no_cache else load_cache(cache_path, fingerprint)
    # 只留本次仍存在的文件，顺带清掉已删除文件的旧条目
    current: dict[str, list[int]] = {}

    reformatted_count = unchanged_count = failed_count = skipped_count = 0
    # -c 命中“会被真正改写”的文件，最后统一打到 stdout
    needs_change: list[Path] = []

    for f in files:
        key = str(f)
        signature = stat_signature(f)
        if signature is not None and cached.get(key) == signature:
            # 自上次处理后未变，无需再格式化
            current[key] = signature
            skipped_count += 1
            continue
        try:
            changed = format_file(f, write=args.inplace)
        except (FormatError, OSError) as e:
            # 单文件失败不中断整批；失败不记缓存，下次重试
            report_progress(f"格式化失败：{f}：{indent_detail(str(e))}", to_stderr=True)
            failed_count += 1
            continue
        # 记录处理后的签名。-c 对“会被改写”的文件不改盘，其盘上仍是待改
        # 状态，故不得当作已格式化写入缓存，否则下次会误跳过。
        if not changed or args.inplace:
            if (written := stat_signature(f)) is not None:
                current[key] = written
        if changed:
            if args.check:
                needs_change.append(f)
            else:
                report_progress(f"已重新格式化：{f}")
                reformatted_count += 1
        else:
            unchanged_count += 1

    save_cache(cache_path, fingerprint, current)

    if args.check:
        # stdout 只输出会被真正改写的文件，便于管道/CI
        for f in needs_change:
            print(f)
        print(file=sys.stderr)  # 与逐文件消息之间空一行（消息也走 stderr）
        if needs_change or failed_count > 0:
            print("有待格式化! 💥 💔 💥", file=sys.stderr)
        else:
            print("全部符合格式! ✨ 🍰 ✨", file=sys.stderr)
        if unchanged_count > 0:
            print(f"{unchanged_count} 个文件无变化。", file=sys.stderr)
        if skipped_count > 0:
            print(f"{skipped_count} 个文件已跳过（上次处理后未变）。", file=sys.stderr)
        if needs_change:
            print(f"{len(needs_change)} 个文件会被改写。", file=sys.stderr)
        if failed_count > 0:
            print(f"{failed_count} 个文件失败。", file=sys.stderr)
        # 有待改文件或出错即非零退出，可作 CI 门禁
        if needs_change or failed_count > 0:
            sys.exit(1)
        return

    if _progress_printed:
        print()  # 与逐文件输出之间空一行

    print("未能完成! 💥 💔 💥" if failed_count > 0 else "全部完成! ✨ 🍰 ✨")

    summary_parts = []
    if reformatted_count > 0:
        summary_parts.append(f"已格式化 {reformatted_count} 个文件")
    if unchanged_count > 0:
        summary_parts.append(f"{unchanged_count} 个文件无变化")
    if skipped_count > 0:
        summary_parts.append(f"{skipped_count} 个文件已跳过（上次处理后未变）")
    if failed_count > 0:
        summary_parts.append(f"{failed_count} 个文件失败")
    if summary_parts:
        print("，".join(summary_parts) + "。")
    else:
        print("未发现需要格式化的文件。")

    if failed_count > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
