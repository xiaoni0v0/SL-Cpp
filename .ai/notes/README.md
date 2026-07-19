# AI 笔记索引

这个目录是 AI（Claude）给自己留的工作笔记，跟 `.ai/context.md`（设计决策/被否决方案/bug 根因分析
的正式记录）是两回事——这里存的是"怎么跟这个项目打交道"的经验，`context.md` 存的是"这个设计为什么
长这样"。两者都随项目文件夹本身走，不依赖任何本机的、位于文件夹之外的记忆系统——换到别的电脑上，
只要这个文件夹本身还在，新开一个 AI 会话读这里就能无缝接上，不需要额外同步任何外部状态。

**新会话/新机器接手时，建议按顺序读**：
1. 项目根目录的 `CLAUDE.md`（会被自动加载，写了最高频、最不能忘的硬约束）
2. 这份索引，按需点开下面链接的具体笔记
3. `.ai/context.md`（体量大，按需搜索关键词读，不用通读）

## 笔记列表

- [build-and-test.md](build-and-test.md) — 怎么自己构建、跑测试，不用求助用户代跑
- [collaboration-conventions.md](collaboration-conventions.md) — 语言/git/并发编辑/写作禁忌/汇报风格这些硬规矩
- [parser-member-vs-lambda.md](parser-member-vs-lambda.md) — Parser.cpp 里辅助逻辑该当类成员还是局部 lambda 的判断标尺
- [parser-prefer-expect.md](parser-prefer-expect.md) — 消耗类型确定的 token 一律用 expect(X)，不用裸 advance()
- [parser-paren-depth-convention.md](parser-paren-depth-convention.md) — finish_* 系列函数管理 paren_depth_ 和括号收尾的统一约定
- [json-test-brace-init-trap.md](json-test-brace-init-trap.md) — 写 parser 测试时 nlohmann::json 花括号初始化会被吃成数组的坑

## 记录新笔记的原则

- 只记"不看代码就推不出来"的东西：用户明确给过的、非显而易见的指令/纠正/确认；被否决过的设计方案
  和否决理由；踩过的坑和根因。**能从读代码本身推出来的（架构、命名、目前用了什么库）不用记**，
  代码变了笔记就会跟着过时，反而误导人。
- 每条笔记独立成文件，文件名用 kebab-case、见名知意；在这份 README 里加一行索引，格式
  `- [file.md](file.md) — 一句话说清楚这条笔记是干嘛的`。
- 笔记内容发现过时/被推翻了，直接改这个文件本身（或者在文件里加一段"后续更新"），不要留着旧结论
  不管——过时笔记比没有笔记更糟。
