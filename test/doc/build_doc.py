"""
SL.md 文档工具。用法：python build_doc.py

  1. 提取多级标题（跳过 ``` 代码块与 $$ 数学块），校验井号数与标题号是否匹配
     （点数 == 井号数 - 2），以及编号是否逐个递增、不漏。
  2. 生成 SL_linked.md：原文复制 + 每个标题前插入不可见锚点 + [TOC] 换成手写目录。
  3. 挖出交叉引用（形如 3.5 / 2.1.4 的节号）并链到对应锚点。
  4. 用 markdown-it-py 渲染出自包含的 SL_linked.html。

依赖：pip install markdown-it-py pygments
"""

import os
import re

from markdown_it import MarkdownIt
from pygments import highlight as pyg_highlight
from pygments.lexers import get_lexer_by_name
from pygments.formatters import HtmlFormatter
from pygments.util import ClassNotFound

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "../..", "SL.md"))
MD_DST = os.path.join(HERE, "SL_linked.md")
HTML_DST = os.path.join(HERE, "SL_linked.html")

HEADER_RE = re.compile(r"^(#{1,6})\s+(\d+(?:\.\d+)*)\s+(.*\S)\s*$")
FENCE_RE = re.compile(r"^\s*```")  # 代码块围栏
MATH_RE = re.compile(r"^\s*\$\$\s*$")  # $$ 数学块


def sec_id(num):
    """节号 -> 锚点 id，例如 '2.1.3' -> 'sec-2-1-3'。"""
    return "sec-" + num.replace(".", "-")


# ---------------- 解析 ----------------
def scan_lines(lines):
    """
    逐行标注是否处于「代码块 / 数学块」内部（含围栏行本身）。
    用于规避代码块里以 # 开头的注释行被误当成标题。
    """
    in_code = []
    fence = math = False
    for ln in lines:
        if fence:
            in_code.append(True)
            if FENCE_RE.match(ln):
                fence = False
            continue
        if math:
            in_code.append(True)
            if MATH_RE.match(ln):
                math = False
            continue
        if FENCE_RE.match(ln):
            fence = True
            in_code.append(True)
            continue
        if MATH_RE.match(ln):
            math = True
            in_code.append(True)
            continue
        in_code.append(False)
    return in_code


def extract_headers(lines, in_code):
    """返回 [(行号0基, level, num, title)]，只取代码块外的编号标题。"""
    headers = []
    for i, ln in enumerate(lines):
        if in_code[i]:
            continue
        m = HEADER_RE.match(ln)
        if m:
            headers.append((i, len(m.group(1)), m.group(2), m.group(3)))
    return headers


# ---------------- 1. 校验 ----------------
def valid_successor(prev, cur):
    """
    大纲编号递增规则，prev/cur 为 int 元组。合法后继当且仅当满足其一：
      - 首个子节：cur == prev + (1,)                （如 1.2 -> 1.2.1）
      - 兄弟/叔辈兄弟：存在 k(1..len(prev))，使
        cur == prev[:k-1] + (prev[k-1]+1,)          （如 1.2->1.3, 1.2.1->1.3, ...->2）
    """
    if cur == prev + (1,):
        return True
    return any(
        cur == prev[: k - 1] + (prev[k - 1] + 1,) for k in range(1, len(prev) + 1)
    )


def check(headers):
    problems = []

    # (a) 井号数 vs 点数：点数应 == 井号数 - 2
    for i, level, num, _ in headers:
        dots = num.count(".")
        if dots != level - 2:
            problems.append(
                f"[井号/点数不符] 第 {i + 1} 行：{'#' * level} {num} "
                f"（{level} 个井号应对应 {level - 2} 个点，实为 {dots} 个）"
            )

    # (b) 递增 / 不漏
    prev = None
    for i, _, num, _ in headers:
        cur = tuple(int(x) for x in num.split("."))
        if prev is None:
            if cur != (1,):
                problems.append(
                    f"[起始编号异常] 第 {i + 1} 行：首个标题为 {num}，期望 1"
                )
        elif not valid_successor(prev, cur):
            problems.append(
                f"[编号不连续] 第 {i + 1} 行：{'.'.join(map(str, prev))} -> {num}"
            )
        prev = cur
    return problems


# ---------------- 2. 目录 ----------------
def build_toc(headers):
    out = ["## 目录", ""]
    for _, level, num, title in headers:
        indent = "  " * (level - 2)  # 章（level 2）不缩进
        out.append(f"{indent}- [{num} {title}](#{sec_id(num)})")
    out.append("")
    return out


# ---------------- 3. 交叉引用 ----------------
# 候选节号：至少含一个点，前后不接数字或点（避免截断更长的数字 / 浮点）
XREF_RE = re.compile(r"(?<![\d.])\d+(?:\.\d+)+(?![\d.])")


def link_xrefs(text, valid_nums):
    """
    在一行正文里把「命中真实存在的节号」的 token 换成 md 超链接。
    按反引号切分，只动行内代码之外的片段。返回 (新文本, 命中数)。
    """
    hits = [0]

    def _sub(m):
        tok = m.group(0)
        if tok in valid_nums:
            hits[0] += 1
            return f"[{tok}](#{sec_id(tok)})"
        return tok

    parts = text.split("`")
    for idx in range(0, len(parts), 2):  # 偶数下标 = 行内代码之外
        parts[idx] = XREF_RE.sub(_sub, parts[idx])
    return "`".join(parts), hits[0]


def build_markdown(lines, in_code, headers):
    """交叉引用 -> 插锚点 -> 替换 [TOC]，返回 (md 文本, 交叉引用数)。"""
    header_at = {i: num for (i, _, num, _) in headers}
    valid_nums = {num for (_, _, num, _) in headers}

    # 先在正文上做交叉引用替换（此时尚未插入目录/锚点，避免误伤）
    total = 0
    body = list(lines)
    for i, ln in enumerate(lines):
        if in_code[i] or i in header_at or ln.strip() == "[TOC]":
            continue
        body[i], n = link_xrefs(ln, valid_nums)
        total += n

    # 再插锚点、替换 [TOC]
    toc = build_toc(headers)
    out = []
    for i, ln in enumerate(body):
        if not in_code[i] and ln.strip() == "[TOC]":
            out.extend(toc)
            continue
        if not in_code[i] and i in header_at:
            out.append(f'<a id="{sec_id(header_at[i])}"></a>')
            out.append("")
        out.append(ln)
    return "\n".join(out), total


# ---------------- 4. 渲染 HTML ----------------
CSS = """
:root { color-scheme: light dark; }
body { max-width: 900px; margin: 0 auto; padding: 2rem 1.2rem;
       font-family: -apple-system, "Segoe UI", "Microsoft YaHei", sans-serif;
       line-height: 1.7; color: #24292f; background: #fff; }
h1,h2,h3,h4,h5,h6 { line-height: 1.25; margin-top: 1.6em; font-weight: 600; }
h1 { border-bottom: 2px solid #d0d7de; padding-bottom: .3em; }
h2 { border-bottom: 1px solid #d0d7de; padding-bottom: .3em; }
a { color: #0969da; text-decoration: none; }
a:hover { text-decoration: underline; }
code { background: rgba(175,184,193,.2); padding: .15em .35em; border-radius: 6px;
       font-family: "Cascadia Mono", Consolas, "Courier New", monospace; font-size: .9em;
       /* 关闭编程连字，避免 <= 显示成 ≤ 等 */
       font-variant-ligatures: none; font-feature-settings: "liga" 0, "calt" 0; }
pre { background: #f6f8fa; padding: 1rem; border-radius: 8px; overflow: auto;
      font-family: "Cascadia Mono", Consolas, "Courier New", monospace;
      font-variant-ligatures: none; font-feature-settings: "liga" 0, "calt" 0; }
pre code { background: none; padding: 0; }
blockquote { border-left: .25em solid #d0d7de; padding: 0 1em; color: #57606a; margin: 0; }
table { border-collapse: collapse; }
th,td { border: 1px solid #d0d7de; padding: .4em .8em; }
:target { scroll-margin-top: 1rem; }
@media (prefers-color-scheme: dark) {
  body { color: #e6edf3; background: #0d1117; }
  h1,h2 { border-color: #30363d; }
  a { color: #4493f8; }
  code { background: rgba(110,118,129,.4); }
  pre { background: #161b22; }
  blockquote { border-color: #30363d; color: #8b949e; }
  th,td { border-color: #30363d; }
}
"""

MATHJAX = """
<script>window.MathJax={tex:{displayMath:[['$$','$$']]},svg:{fontCache:'global'}};</script>
<script src="https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js" async></script>
"""

MATH_BLOCK_RE = re.compile(r"^\$\$\s*$.*?^\$\$\s*$", re.S | re.M)


def _highlight(code, lang, attrs):
    """已知语言用 Pygments 高亮；未知语言返回 '' 让 md 走默认转义。"""
    try:
        lexer = get_lexer_by_name(lang)
    except ClassNotFound:
        return ""
    return pyg_highlight(code, lexer, HtmlFormatter(noclasses=True))


def render_html(text):
    """
    markdown-it-py（CommonMark + GFM 表格）：
      - 表格单元格内的 \\| 按 GFM 规范还原成 |；
      - breaks=True：单个换行即渲染成换行，无需末尾双空格；
      - CommonMark 列表续行规则：2 空格缩进的续行不会打断列表；
      - html=True：放行 <a id> 锚点。
    $$...$$ 数学块先抠出占位、渲染后原样塞回，交给页面里的 MathJax。
    """
    maths = []

    def _stash(m):
        maths.append(m.group(0))
        return f"\n\nMATHPLACEHOLDER{len(maths) - 1}ENDPLACEHOLDER\n\n"

    text = MATH_BLOCK_RE.sub(_stash, text)

    md = MarkdownIt(
        "commonmark",
        {
            "html": True,
            "breaks": True,
            "highlight": _highlight,
        },
    ).enable("table")
    body = md.render(text)

    def _restore(m):
        return maths[int(m.group(1))]

    body = re.sub(r"<p>MATHPLACEHOLDER(\d+)ENDPLACEHOLDER</p>", _restore, body)
    body = re.sub(r"MATHPLACEHOLDER(\d+)ENDPLACEHOLDER", _restore, body)

    return (
        '<!DOCTYPE html>\n<html lang="zh-CN">\n<head>\n'
        '<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
        "<title>SL 语言规范</title>\n"
        f"<style>{CSS}</style>\n{MATHJAX}</head>\n<body>\n"
        f"{body}\n</body>\n</html>\n"
    )


# ---------------- 主流程 ----------------
def main():
    with open(SRC, "r", encoding="utf-8") as f:
        lines = f.read().split("\n")

    in_code = scan_lines(lines)
    headers = extract_headers(lines, in_code)

    problems = check(headers)
    if problems:
        print(f"标题 {len(headers)} 个，编号校验发现 {len(problems)} 处问题：")
        for p in problems:
            print("  " + p)
    else:
        print(f"标题 {len(headers)} 个，编号校验通过")

    md_text, xrefs = build_markdown(lines, in_code, headers)
    # newline="\n"：SL.md 本身是 LF，输出跟它保持一致，不受运行平台的文本模式换行转换影响
    # （不加这个，Windows 上文本模式写入会把 \n 转成 \r\n，生成结果就跟平台绑定、不可复现）
    with open(MD_DST, "w", encoding="utf-8", newline="\n") as f:
        f.write(md_text)

    with open(HTML_DST, "w", encoding="utf-8", newline="\n") as f:
        f.write(render_html(md_text))

    print(f"交叉引用 {xrefs} 处")
    print(f"已生成 {os.path.basename(MD_DST)}、{os.path.basename(HTML_DST)}")


if __name__ == "__main__":
    main()
