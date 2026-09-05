# 编码一律 UTF-8 无 BOM

**约定**：仓库里所有文本文件都是 UTF-8，不带 BOM。这条由 `format.py` 的闸门保证（见
`ensure_utf8` 和 `format_file`）：读进来的内容不是合法 UTF-8 就抛 `FormatError` 跳过，**绝不猜
编码、绝不代为转换**；带 UTF-8 BOM 的会在进出格式化器时各剥一次，写回的一定是无 BOM 版本。

姊妹约定见 [line-endings.md](line-endings.md)（行尾一律 LF）。

**why**：项目的代码注释全是中文，等于每个源文件都带非 ASCII 字节，编码选错影响面是 100%。GBK 出了
简体中文 Windows 就没人认，而 Git/GitHub、CMake、Python、Node 全部默认 UTF-8。

真正促使把闸门做进 `format.py` 的，是各个工具对非 UTF-8 的反应**并不一致**，其中一条会丢数据：

| 混进来的东西 | 谁拦得住 | 结果 |
| --- | --- | --- |
| `.cpp` 是 GBK，中文在**字符串字面量**里 | clang-cl | `-Winvalid-source-encoding`，项目开了 `/WX`，构建直接失败 |
| `.cpp` 是 GBK，中文**只在注释**里 | 没有人 | 编译 `rc=0` 静默通过；clang-format 把字节原样透传，也不报错 |
| `.py` 是 GBK | black | `rc=123 invalid or missing encoding declaration`，拒绝 |
| `CMakeLists.txt` 是 GBK | gersemi | `rc=123 stream did not contain valid UTF-8`，拒绝 |
| **`.md` 是 GBK** | **没有人** | **prettier 把非法字节解成一串 U+FFFD 再输出，中文全变 `锟斤拷`，而且 `rc=0`** |

最后一行是关键：`format.py` 只看退出码的话会把这堆垃圾当成正常格式化结果写回去，原文永久丢失。
「退出码 0 但输出为空」那道防线也拦不住它——它的输出非空，只是内容已经毁了。所以编码必须在**跑
格式化器之前**验，而不是事后检查输出。

**how to apply**：

- 新建文件一律存 UTF-8 无 BOM。Windows 上要留神：PowerShell 的 `>` 重定向、记事本另存为、某些
  编辑器的"UTF-8 with BOM"默认项都会悄悄加 BOM。
- 发现 `format.py` 报「不是合法的 UTF-8」，**不要**图省事把闸门绕过去或者让脚本自动转——先弄清楚
  这个文件是怎么变成非 UTF-8 的，手动转好再跑。自动猜编码猜错一次就是不可逆的内容损坏。
- BOM 不用手动处理：四个格式化器（clang-format / black / prettier / gersemi）**全都原样保留
  BOM**，所以光跑格式化清不掉，但 `format.py` 的闸门会剥。
- BOM 本身不影响编译（clang-cl 对带 BOM 的源文件 `rc=0`），也不影响 `.clang-format`/`.gersemirc`
  这些配置文件被正确读取——之所以还是要清掉，是因为它污染 diff、破坏 `#!` shebang、在 grep 和正则
  里制造一个看不见的首字符。
- 闸门只覆盖 `format.py` 收录的那些文件名/后缀。`.gitattributes`、`.gersemirc`、`.gitignore`
  这类不在分派表里的盖不到，但实测配置文件对 BOM 都不敏感（clang-format 和 gersemi 读带 BOM 的
  配置照样正常），风险可以接受。
