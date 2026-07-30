# SL-Cpp 项目须知

SL 是一门自定义脚本语言，本仓库是它的 C++ 实现（词法/语法分析器已实现，语义分析/表达式折叠正在做，
执行器/虚拟机是空壳，还没写）。语言规范在 [SL.md](SL.md)。

**在做任何实质性工作之前，先读**：
1. [.ai/notes/README.md](.ai/notes/README.md) —— AI 自己积累的工作笔记索引（怎么构建测试、代码风格
   约定、踩过的坑）。
2. [.ai/context.md](.ai/context.md) —— 设计决策、被否决过的方案、bug 根因分析的完整记录（体量大，
   按关键词搜索着读，不用通读）。

这两处笔记跟这个项目文件夹本身走，不依赖任何本机的、文件夹之外的记忆系统——文件夹搬到别的电脑上，
这里的内容原样都在。

## 最高频、不能忘的硬约束

- **全程中文交流**；代码注释也用中文，标识符/关键字用英文。
- **不准动 git**：不主动执行任何 git 命令（只读命令也不例外），除非用户明确要求。
- 改完 `parser/`/`lexer/` 代码后，自己用 `cmd //c "E:\Programs\SL-Cpp\.ai\run_test.bat"` 构建并跑
  测试验证，不要让用户代跑。细节见 [.ai/notes/build-and-test.md](.ai/notes/build-and-test.md)。
- 用户会一边对话一边自己直接改代码；遇到"文件被修改过"的提示，正常按新内容继续干活，不要撤销、也
  不用特意提。
- 除 `.ai/context.md` 外，代码/文档里禁止出现"此次对话""刚刚加入"这类引用当前会话本身的措辞。

完整版协作规矩（汇报风格、测试写法惯例等）见
[.ai/notes/collaboration-conventions.md](.ai/notes/collaboration-conventions.md)。
