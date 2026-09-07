# AI 笔记索引

这个目录存具体的、单一主题的约定/踩过的坑，是 `.ai/` 三件套里最细粒度的一层：
[architecture.md](../architecture.md) 讲"代码在哪、整体怎么组织"，[context.md](../context.md) 讲
"语言设计为什么长这样"，这里讲"写这个代码库的代码时，某个具体地方该怎么做、为什么"。三者都随项目
文件夹本身走，不依赖任何本机的、位于文件夹之外的记忆系统——换到别的电脑上，只要这个文件夹本身还
在，新开一个 AI 会话读这里就能无缝接上。

**新会话/新机器接手时，建议按顺序读**：
1. 项目根目录的 `CLAUDE.md`（会被自动加载，写了最高频、最不能忘的硬约束）
2. [architecture.md](../architecture.md)（项目结构、代码索引，从这里开始摸清代码在哪）
3. 这份索引，按需点开下面链接的具体笔记
4. [context.md](../context.md)（设计决策/被否决方案/悬而未决的问题，体量大，按需搜索关键词读）

## 笔记列表

- [build-and-test.md](build-and-test.md) — 怎么自己构建、跑测试，不用求助用户代跑
- [line-endings.md](line-endings.md) — 行尾一律 LF、仓库不存 `.bat`/`.cmd`，以及这是靠什么保证的
- [encoding.md](encoding.md) — 编码一律 UTF-8 无 BOM，以及为什么这道闸门必须卡在跑格式化器之前
- [collaboration-conventions.md](collaboration-conventions.md) — 语言/git/并发编辑/写作禁忌/汇报风格这些硬规矩
- [user-facing-strings.md](user-facing-strings.md) — 异常信息/assert/终端输出一律英文且简短，以及简洁到什么程度
- [no-section-numbers.md](no-section-numbers.md) — 代码/测试/目录名里一律不许出现 SL.md 章节号，只能写文字描述
- [parser-member-vs-lambda.md](parser-member-vs-lambda.md) — Parser.cpp 里辅助逻辑该当类成员还是局部 lambda 的判断标尺
- [parser-prefer-expect.md](parser-prefer-expect.md) — 消耗类型确定的 token 一律用 expect(X)，不用裸 advance()
- [parser-paren-depth-convention.md](parser-paren-depth-convention.md) — finish_* 系列函数管理括号栈 brackets_ 和括号收尾的统一约定
- [parser-token-vs-expr-level.md](parser-token-vs-expr-level.md) — Parser 某个槽位该收紧到 token 级别还是走通用表达式再交给语义层判形状
- [ast-node-passing-conventions.md](ast-node-passing-conventions.md) — AST 节点该传引用、非 const 引用还是智能指针引用，以及 const 在这里承载的信息
- [visitor-result-passing.md](visitor-result-passing.md) — AST 访问者要返回值/带参数时怎么手搓成员通道，以及为什么不抽成模板基类
- [no-out-params.md](no-out-params.md) — 辅助函数不用引用形参改外边、一律 return，以及哪些 in-out 语义不在此列
- [object-model-conventions.md](object-model-conventions.md) — runtime/ 里 C++ 类继承与 SL 类继承的分工，以及 Ref/裸指针的所有权约定
- [cpp-layer-vs-sl-layer.md](cpp-layer-vs-sl-layer.md) — 哪一层才能命名 SL 异常/碰对象模型，底层出错该怎么往上转
- [json-test-brace-init-trap.md](json-test-brace-init-trap.md) — 写 parser 测试时 nlohmann::json 花括号初始化会被吃成数组的坑
- [grok-dsh-cli-review.md](grok-dsh-cli-review.md) — 调用本机 grok/dsh 两个 CLI 做交叉代码审查时的只读参数，避免卡在权限提示上

## 记录新笔记的原则

- 只记"不看代码就推不出来、或者读代码要花不少功夫才能拼出来"的东西：非显而易见的约定和它的
  why/how to apply；被否决过的具体做法和理由；容易反复踩的坑和根因。
- **不是"能从代码推出来就不用记"，是"别把笔记写成代码的复述"**：一条约定该不该记，看的是"下次
  有人（AI 或人）在这个代码库写类似代码时，不知道这条约定会不会写出风格不一致或者更容易出 bug的
  东西"，不是看"这条约定理论上能不能从盯着代码看推出来"——像 `parser-member-vs-lambda.md`、
  `parser-token-vs-expr-level.md` 这类判断标尺，本质是"以后加代码时套用的取舍规则"，不是描述
  现状，永远该记。真正不该记的是纯粹复述现状的东西（"`Parser.cpp` 有一个 `parse_for` 函数"这种），
  这类导航性内容属于 [architecture.md](../architecture.md)。
- 每条笔记独立成文件，文件名用 kebab-case、见名知意；在这份 README 里加一行索引，格式
  `- [file.md](file.md) — 一句话说清楚这条笔记是干嘛的`。
- 只写规则本身和 why/how to apply，不写"用户第几次会话怎么发现的""改了几次才对"这类过程性叙事——
  这些是 git log 的职责。
- 笔记内容发现过时/被推翻了，直接改这个文件本身，不要留着旧结论不管——过时笔记比没有笔记更糟。
