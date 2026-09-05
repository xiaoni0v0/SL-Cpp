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

跳过没动过的文件：-i 模式下，每个文件处理完就把它的 (修改时间, 大小) 记进脚本同目录的
CACHE_FILENAME；下次跑时对得上就直接跳过，不再起格式化器进程。只要格式化器版本、格式化配置
或本脚本自身有任何变化，整份缓存立即作废、全部重跑（见 environment_fingerprint）。

用法：
    python format.py [项目根目录] [-i] [--no-cache]

参数：
    项目根目录       默认是当前目录 "."
    -i, --in-place   真正原地改写文件；不加这个参数只列出会被本脚本覆盖的文件
    --no-cache       忽略缓存，本次强制重新格式化每个文件（只在 -i 模式下有意义）
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
# 缓存文件放在本脚本旁边。改了缓存的字段含义就把版本号 +1，旧缓存会自动作废
CACHE_FILENAME = ".format_cache.json"
CACHE_VERSION = 1
# 排除的文件名
EXCLUDE_FILE_PATTERNS = [
    "big_int_cases.inc",  # test/numeric/gen_big_int_cases.py 生成
    "big_dec_cases.inc",  # test/numeric/gen_big_dec_cases.py 生成
    CACHE_FILENAME,  # 本脚本自己的缓存：格式化完立刻被 save_cache 覆盖，纯属白跑
]
# 单个格式化器进程的超时（秒），防止某个工具挂死后整批无声无息地卡住
TIMEOUT_SECONDS = 60
# UTF-8 BOM。项目统一 UTF-8 无 BOM，读进来遇到就剥掉
UTF8_BOM = b"\xef\xbb\xbf"
# 影响所有文件格式化结果的配置文件，纳入环境指纹
CONFIG_FILENAMES = [".clang-format", ".gersemirc"]


@functools.lru_cache(maxsize=None)
def which_cached(command: str) -> str | None:
    """
    shutil.which 要把 PATH 扫一遍，实测 200 次就是 0.34 秒。
    每个文件都查一次的话，这点开销在缓存全命中时反而成了大头，所以查过就记住。
    """
    return shutil.which(command)


class Formatter(NamedTuple):
    """一个格式化器：叫什么、怎么拼命令行、内容走 stdin 还是给路径"""

    command: str  # 可执行文件名，用于 PATH 检查和报错
    build_argv: Callable[[Path], list[str]]
    via_stdin: bool  # True 表示把原文件内容喂给 stdin
    install_hint: str  # 没装时告诉用户怎么装

    def run(self, path: Path, original: bytes) -> bytes:
        """跑一遍，返回格式化后的内容；失败抛 FormatError"""
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
    # Formatter 是 NamedTuple，也就是 tuple，真值性取决于字段个数——这里只能显式跟 None 比
    by_name = FORMATTERS_BY_NAME.get(path.name.lower())
    if by_name is not None:
        return by_name
    return FORMATTERS.get(path.suffix.lower())


def find_target_files(project_dir: Path) -> list[Path]:
    """查找所有待格式化的文件，跳过排除目录（整棵子树）和排除文件"""
    files = []
    for dirpath, dirnames, filenames in os.walk(project_dir):
        # 就地裁剪
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


def write_atomic(path: Path, data: bytes) -> None:
    """
    先写同目录的临时文件、再 os.replace 顶上去。

    直接 write_bytes 是"先截断再写"，中途断电、磁盘满、Ctrl+C，留下的就是半截甚至空的源文件，
    原内容再也找不回来。临时文件必须跟目标同目录，跨盘的 os.replace 不是原子操作。
    """
    temporary = path.with_name(path.name + ".format-tmp")
    try:
        temporary.write_bytes(data)
        os.replace(temporary, path)
    except OSError:
        temporary.unlink(missing_ok=True)
        raise


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
        write_atomic(path, formatted)
        return True
    return False


def pluralize(count: int, noun: str) -> str:
    return f"{count} {noun}" if count == 1 else f"{count} {noun}s"


def needed_formatters(files: list[Path]) -> list[Formatter]:
    """这批文件用得到的格式化器，按命令名排序（排序是为了让环境指纹稳定）"""
    by_command = {formatter_for(f).command: formatter_for(f) for f in files}
    # 只拿 command 排序：sorted(items()) 在 command 撞车时会去比 Formatter，而它装着 lambda，比不了
    return [by_command[command] for command in sorted(by_command)]


def missing_formatters(files: list[Path]) -> list[Formatter]:
    """这批文件用得到、但 PATH 里找不到的格式化器"""
    return [fm for fm in needed_formatters(files) if which_cached(fm.command) is None]


def stat_signature(path: Path | None) -> list[int] | None:
    """文件的 (修改时间, 大小)。取不到就是 None——文件不在、或者压根没给路径"""
    if path is None:
        return None
    try:
        info = path.stat()
    except OSError:
        return None
    return [info.st_mtime_ns, info.st_size]


def environment_fingerprint(project_dir: Path, formatters: list[Formatter]) -> str:
    """
    把"会影响格式化结果的外部因素"压成一个字符串：本脚本自身、这批文件用到的格式化器
    可执行文件、项目里的格式化配置。其中任何一项变了，上次记下的结果就不能再信，
    整份缓存作废、全部重跑。

    这里用可执行文件的 (路径, 修改时间, 大小) 代替 `--version`，是因为四个工具各跑一次
    --version 要 0.38 秒，比它省下来的还多。代价是：如果哪天升级工具没换掉可执行文件本身，
    这里就察觉不到，那种情况下用 --no-cache 跑一次。
    """
    parts = [f"v{CACHE_VERSION}", f"self={stat_signature(Path(__file__).resolve())}"]
    for fm in formatters:
        exe = which_cached(fm.command)
        signature = stat_signature(Path(exe)) if exe else None
        parts.append(f"{fm.command}={exe}@{signature}")
    for name in CONFIG_FILENAMES:
        parts.append(f"{name}@{stat_signature(project_dir / name)}")
    return "|".join(parts)


def cache_path() -> Path:
    """
    缓存固定放在本脚本旁边，不跟着被格式化的目录走。所以拿这个脚本去格式化另一个目录，
    会把上一个目录的记录整个顶掉，下次回来得全量重跑一遍——只是慢一次，不影响正确性。
    """
    return Path(__file__).resolve().parent / CACHE_FILENAME


def load_cache(fingerprint: str) -> dict[str, list[int]]:
    """
    读上次记下的 (修改时间, 大小)。
    文件不在、内容坏了、或者环境指纹对不上，一律当作没有缓存——宁可白跑一遍，也不能
    拿着过时的结论跳过该格式化的文件。
    """
    try:
        with cache_path().open(encoding="utf-8") as fp:
            data = json.load(fp)
    except (OSError, ValueError):
        return {}
    if not isinstance(data, dict) or data.get("fingerprint") != fingerprint:
        return {}
    entries = data.get("files")
    return entries if isinstance(entries, dict) else {}


def save_cache(fingerprint: str, entries: dict[str, list[int]]) -> None:
    """写缓存。先写临时文件再替换，免得中途挂掉留下半截 JSON"""
    temporary = cache_path().with_name(CACHE_FILENAME + ".tmp")
    try:
        with temporary.open("w", encoding="utf-8", newline="\n") as fp:
            json.dump(
                {"fingerprint": fingerprint, "files": entries},
                fp,
                ensure_ascii=False,
                indent=1,
                sort_keys=True,
            )
        os.replace(temporary, cache_path())
    except OSError as e:
        # 缓存写不成不影响这次格式化的正确性，顶多下次白跑一遍
        print(
            f"警告：缓存没能写进 {cache_path()}（{e}），下次会全量重跑", file=sys.stderr
        )


def main() -> None:
    parser = argparse.ArgumentParser(description="批量格式化项目源文件")
    parser.add_argument(
        "project_dir", nargs="?", default=".", help="项目根目录，默认为当前目录"
    )
    parser.add_argument(
        "-i",
        "--in-place",
        action="store_true",
        help="真正原地改写文件；不加这个参数只列出会被本脚本覆盖的文件",
    )
    parser.add_argument(
        "--no-cache",
        action="store_true",
        help="忽略缓存，本次强制重新格式化每个文件（只在 -i 模式下有意义）",
    )
    args = parser.parse_args()
    if args.no_cache and not args.in_place:
        # 不加 -i 时压根不碰缓存，这个组合什么也不做——与其静默无效，不如当场说清楚
        parser.error("--no-cache 只在 -i 模式下有意义：不加 -i 时本来就不读写缓存")

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
        # 默认模式：stdout 上只有命中的文件名，方便管道接别的命令
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

    fingerprint = environment_fingerprint(project_dir, needed_formatters(files))
    cached = {} if args.no_cache else load_cache(fingerprint)
    # 只装这次真正见到的文件，顺带把已经删掉的文件的陈旧条目清理出去
    current: dict[str, list[int]] = {}

    reformatted_count = unchanged_count = failed_count = skipped_count = 0

    for f in files:
        key = str(f)
        signature = stat_signature(f)
        if signature is not None and cached.get(key) == signature:
            # 上次处理完之后没人动过它，格式化器进程都不用起
            current[key] = signature
            skipped_count += 1
            continue
        try:
            changed = format_file(f)
        except (FormatError, OSError) as e:
            # 单个文件失败不中断整批，最后统一汇总并以非零码退出。
            # 失败的不记进缓存，下次还得重来一遍
            report_progress(
                f"错误：格式化失败 {f}：{indent_detail(str(e))}", to_stderr=True
            )
            failed_count += 1
            continue
        # 记的是处理完之后的状态：文件刚被改写过的话，时间和大小都变了
        if (written := stat_signature(f)) is not None:
            current[key] = written
        if changed:
            report_progress(f"reformatted {f}")
            reformatted_count += 1
        else:
            unchanged_count += 1

    save_cache(fingerprint, current)

    if _progress_printed:
        print()  # 把上面那堆逐文件的输出和总结行隔开

    print("Oh no! 💥 💔 💥" if failed_count > 0 else "All done! ✨ 🍰 ✨")

    summary_parts = []
    if reformatted_count > 0:
        summary_parts.append(pluralize(reformatted_count, "file") + " reformatted")
    if unchanged_count > 0:
        summary_parts.append(pluralize(unchanged_count, "file") + " left unchanged")
    if skipped_count > 0:
        summary_parts.append(
            pluralize(skipped_count, "file") + " skipped (unchanged since last run)"
        )
    if failed_count > 0:
        summary_parts.append(pluralize(failed_count, "file") + " failed to reformat")
    print((", ".join(summary_parts) + ".") if summary_parts else "No files matched.")

    if failed_count > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
