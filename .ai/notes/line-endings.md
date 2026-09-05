# 行尾一律 LF，仓库不存 `.bat`/`.cmd`

**约定**：所有文本文件在仓库里和工作区里都是 LF。权威来源是项目根的 `.gitattributes`
（`* text=auto eol=lf`），不是各人本机的 `core.autocrlf`。`text=auto` 会自动识别二进制并跳过转换，
所以新加文件不用做任何事。

配套的两处设置，缺一就会出现"同一个仓库里两套行尾"：

- `.clang-format` 里的 `LineEnding: LF`。不写这条的话 clang-format 默认从输入内容推断行尾，遇到
  CRLF 文件原样保留，而 `format.py` 里的 prettier 和 gersemi 本来就只吐 LF——于是 `.cpp` 和
  `.md` 走两套行为。
- git 那一层。black 没有行尾选项、固定跟随输入的第一行，所以 `.py` 的行尾 clang-format 管不着，
  只能靠 `.gitattributes` 在检出/提交时保证。

**why**：项目要交到 GitHub，diff/blame/PR review 和 Linux 上的工具链全按 LF 算，CRLF 进了仓库
会让别人一改就是"整文件全行改动"的 diff。Windows 这边没有反作用力——MSVC/clang、CMake、Python、
CLion/VS Code 全都原生吃 LF。选 `.gitattributes` 而不是 `core.autocrlf`，是因为后者是每台机器
各自配的：队友配错就污染仓库，而且光看仓库本身根本看不出约定是什么。

**how to apply**：

- **不要往仓库里加 `.bat` / `.cmd`**。批处理是唯一真正还需要 CRLF 的格式（老 cmd.exe 对 LF 换行的
  label/`goto` 有历史坑），而为它在 `.gitattributes` 里开一条 `eol=crlf` 例外，等于在一个统一 LF
  的仓库里长期养一类反例。要写脚本就写 Python：跨平台，而且和项目里已有的 `format.py`、
  `test/doc/build_doc.py`、`test/numeric/gen_*.py` 一致。`.ai/run_test.py` 就是照这条改过来的，
  见 [build-and-test.md](build-and-test.md)。
- 需要在 Python 脚本里拿到 Windows 控制台的 UTF-8 输出（原来 `.bat` 靠 `chcp 65001` 做的事），
  用 `ctypes.windll.kernel32.SetConsoleOutputCP(65001)` 加 `sys.stdout.reconfigure(encoding="utf-8")`，
  `.ai/run_test.py` 里有现成写法。
