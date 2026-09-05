#!/usr/bin/env python3

"""
format.py

批量格式化项目源文件，按完整文件名、其次按后缀分派给对应工具：

    .cpp / .h / .inc                            ->  clang-format
    CMakeLists.txt / .cmake                     ->  gersemi
    .py                                         ->  black
    .md / .json / .yaml / .yml / .clang-format  ->  prettier

其余文件一律忽略。匹配 EXCLUDE_DIR_PATTERNS（整棵子树）或 EXCLUDE_FILE_PATTERNS
的文件不处理。文件一律按 UTF-8 处理：非合法 UTF-8 报错并跳过（不猜测、不转码），
带 BOM 的在写回时剥除。

-i 模式把每个文件处理后的（修改时间, 大小）记入脚本旁的 CACHE_FILENAME，下次
命中则跳过；脚本自身、格式化器或配置变化会使缓存整体失效（见 environment_fingerprint）。

用法：
    python format.py [项目根目录] [-i] [--no-cache]

参数：
    项目根目录      默认当前目录
    -i, --in-place  原地改写文件；缺省只列出将被覆盖的文件
    --no-cache      忽略缓存，强制重跑（仅 -i 模式有意义）
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

# Windows 上 print 默认把 \n 写成 \r\n，且重定向时 stdout 为块缓冲，
# 会让下游管道收到带 \r 的路径并打乱 stdout/stderr 的输出顺序，这里统一修正
for _stream in sys.stdout, sys.stderr:
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(
            encoding="utf-8",
            errors="backslashreplace",
            newline="\n",
            line_buffering=True,
        )

# 命中则整棵子树不处理（glob 通配，大小写不敏感）
EXCLUDE_DIR_PATTERNS = [
    ".ai",
    ".claude",
    ".git",
    ".idea",
    ".venv",
    "cmake-build-*",
]
# 缓存放脚本旁；缓存结构变化时递增版本号使旧缓存失效
CACHE_FILENAME = ".format_cache.json"
CACHE_VERSION = 1
# 命中则跳过
EXCLUDE_FILE_PATTERNS = [
    # 由 test/numeric/gen_*_cases.py 生成
    "big_int_cases.inc",
    "big_dec_cases.inc",
    # 本脚本缓存，处理完即被覆盖
    CACHE_FILENAME,
]
# 单个格式化器进程的超时（秒），防止工具挂起拖住整批
TIMEOUT_SECONDS = 60
# 项目统一 UTF-8 无 BOM，读入时剥除
UTF8_BOM = b"\xef\xbb\xbf"
# 会影响格式结果的配置文件，纳入环境指纹
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
            # TimeoutExpired 不是 OSError，须转成 FormatError 才能被统一捕获
            raise FormatError(
                f"{self.command} 超过 {TIMEOUT_SECONDS} 秒未返回，已放弃该文件"
            ) from None
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        if result.returncode != 0:
            raise FormatError(detail or f"{self.command} 退出码 {result.returncode}")
        if original and not result.stdout:
            # 退出码为 0 却无输出，直接写回会清空源文件
            raise FormatError(detail or f"{self.command} 退出码 0 但无输出")
        if detail:
            # 成功时的告警也要展示，不能被吞掉
            report_progress(
                f"警告：{self.command}（{path}）：{indent_detail(detail)}",
                to_stderr=True,
            )
        return result.stdout


class FormatError(RuntimeError):
    """单个文件格式化失败"""


# 循环中是否已输出逐文件消息，决定结尾是否补空行隔开总结
_progress_printed = False


def report_progress(message: str, *, to_stderr: bool = False) -> None:
    """输出一行逐文件消息"""
    global _progress_printed
    _progress_printed = True
    print(message, file=sys.stderr if to_stderr else sys.stdout)


def indent_detail(detail: str) -> str:
    """把多行信息缩进成整块，避免续行与其它输出混淆"""
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

# 后缀 -> 格式化器；支持新语言时在此追加即可
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
    """大小写不敏感的通配符匹配，保证跨平台行为一致"""
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
    """查找待格式化文件，跳过排除目录（整棵子树）与排除文件"""
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
    """编码闸门：非合法 UTF-8 抛 FormatError 并跳过该文件。

    只拦截不转码。prettier 会把坏字节解成 U+FFFD 且以退出码 0 输出，
    直接写回即损坏原文，故须在调用格式化器之前检查。"""
    try:
        original.decode("utf-8")
    except UnicodeDecodeError as e:
        raise FormatError(
            f"不是合法 UTF-8（第 {e.start} 字节起：{e.reason}），已跳过；"
            f"请转码为 UTF-8 后重新运行"
        ) from None


def write_atomic(path: Path, data: bytes) -> None:
    """先写同目录临时文件再原子替换，避免中断留下残缺内容。

    临时文件须与目标同目录，跨盘替换不是原子操作。"""
    temporary = path.with_name(path.name + ".format-tmp")
    try:
        temporary.write_bytes(data)
        os.replace(temporary, path)
    except OSError:
        temporary.unlink(missing_ok=True)
        raise


def format_file(path: Path) -> bool:
    """格式化单个文件；内容有变化则写回并返回 True"""
    original = path.read_bytes()
    ensure_utf8(original)
    # 格式化器会原样保留 BOM，进出各剥一次
    source = original.removeprefix(UTF8_BOM)
    formatted = formatter_for(path).run(path, source).removeprefix(UTF8_BOM)
    if formatted != original:
        write_atomic(path, formatted)
        return True
    return False


def needed_formatters(files: list[Path]) -> list[Formatter]:
    """本批文件所需的格式化器，按命令名排序（保证环境指纹稳定）"""
    by_command = {formatter_for(f).command: formatter_for(f) for f in files}
    # 只对命令名排序再查表，避免比较到含 lambda 的 Formatter
    return [by_command[command] for command in sorted(by_command)]


def missing_formatters(files: list[Path]) -> list[Formatter]:
    """所需但不在 PATH 中的格式化器"""
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
    """把影响格式结果的外部因素压成指纹：脚本自身、格式化器可执行文件、配置文件。

    任一项变化都视为缓存失效。用可执行文件的（路径, 修改时间, 大小）代替
    --version 查询，省去逐工具跑版本的时间；若升级未替换可执行文件则此指纹
    察觉不到变化，此时用 --no-cache 强制重跑。"""
    parts = [f"v{CACHE_VERSION}", f"self={stat_signature(Path(__file__).resolve())}"]
    for fm in formatters:
        exe = which_cached(fm.command)
        signature = stat_signature(Path(exe)) if exe else None
        parts.append(f"{fm.command}={exe}@{signature}")
    for name in CONFIG_FILENAMES:
        parts.append(f"{name}@{stat_signature(project_dir / name)}")
    return "|".join(parts)


def cache_path() -> Path:
    """缓存固定放脚本旁，换目录会互相覆盖记录（仅多一次全量重跑）"""
    return Path(__file__).resolve().parent / CACHE_FILENAME


def load_cache(fingerprint: str) -> dict[str, list[int]]:
    """读缓存；文件缺失、损坏或指纹不符时一律视为无缓存"""
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
    """写缓存：先写临时文件再替换，防止留下半截 JSON"""
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
        # 缓存写失败不影响本次结果，最多下次全量重跑
        print(
            f"警告：缓存写入失败：{cache_path()}（{e}），下次将全量重跑",
            file=sys.stderr,
        )


def main() -> None:
    parser = argparse.ArgumentParser(description="批量格式化项目源文件")
    parser.add_argument(
        "project_dir", nargs="?", default=".", help="项目根目录（默认当前目录）"
    )
    parser.add_argument(
        "-i",
        "--in-place",
        action="store_true",
        help="原地改写文件；缺省只列出将被覆盖的文件",
    )
    parser.add_argument(
        "--no-cache",
        action="store_true",
        help="忽略缓存，强制重跑（仅 -i 模式有意义）",
    )
    args = parser.parse_args()
    if args.no_cache and not args.in_place:
        # 缺省模式不读写缓存，此组合无任何效果
        parser.error("--no-cache 仅在 -i 模式下有意义")

    started_at = time.perf_counter()
    try:
        run(args)
    finally:
        print(f"总耗时 {time.perf_counter() - started_at:.2f} 秒", file=sys.stderr)


def run(args: argparse.Namespace) -> None:
    """参数解析完成后的主流程"""
    project_dir = Path(args.project_dir).resolve()
    if not project_dir.is_dir():
        print(f"错误：目录不存在：{project_dir}", file=sys.stderr)
        sys.exit(1)

    files = find_target_files(project_dir)

    global _progress_printed
    _progress_printed = False  # 多次调用（含 import 复用）也从干净状态开始

    if not args.in_place:
        # 缺省模式：stdout 只输出文件名，便于管道接其它命令
        for f in files:
            print(f)
        return

    if missing := missing_formatters(files):
        print(
            "错误：以下格式化器不在 PATH 中，未处理任何文件。请按提示安装后重新运行：",
            file=sys.stderr,
        )
        for fm in missing:
            print(f"    {fm.command:<14}{fm.install_hint}", file=sys.stderr)
        sys.exit(1)

    fingerprint = environment_fingerprint(project_dir, needed_formatters(files))
    cached = {} if args.no_cache else load_cache(fingerprint)
    # 只记录本次仍存在的文件，同时清除已删除文件的陈旧条目
    current: dict[str, list[int]] = {}

    reformatted_count = unchanged_count = failed_count = skipped_count = 0

    for f in files:
        key = str(f)
        signature = stat_signature(f)
        if signature is not None and cached.get(key) == signature:
            # 文件自上次处理后未变，无需再次格式化
            current[key] = signature
            skipped_count += 1
            continue
        try:
            changed = format_file(f)
        except (FormatError, OSError) as e:
            # 单个文件失败不中断整批；失败文件不记缓存，下次重试
            report_progress(f"格式化失败：{f}：{indent_detail(str(e))}", to_stderr=True)
            failed_count += 1
            continue
        # 记录处理后的状态（已改写的文件时间与大小随之更新）
        if (written := stat_signature(f)) is not None:
            current[key] = written
        if changed:
            report_progress(f"已重新格式化：{f}")
            reformatted_count += 1
        else:
            unchanged_count += 1

    save_cache(fingerprint, current)

    if _progress_printed:
        print()  # 与逐文件输出之间空一行

    print("存在处理失败的文件。" if failed_count > 0 else "全部处理完成。")

    summary_parts = []
    if reformatted_count > 0:
        summary_parts.append(f"已重新格式化 {reformatted_count} 个文件")
    if unchanged_count > 0:
        summary_parts.append(f"{unchanged_count} 个文件内容无变化")
    if skipped_count > 0:
        summary_parts.append(f"{skipped_count} 个文件已跳过（上次处理后未变化）")
    if failed_count > 0:
        summary_parts.append(f"{failed_count} 个文件处理失败")
    if summary_parts:
        print("，".join(summary_parts) + "。")
    else:
        print("未发现需要格式化的文件。")

    if failed_count > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
