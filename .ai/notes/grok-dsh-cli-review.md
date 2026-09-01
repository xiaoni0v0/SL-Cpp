# 调用 grok / dsh 做交叉审查的只读参数

本机上装了两个可以像 subagent 一样调用的第三方 AI CLI：`grok`（xAI Grok Build TUI）和
`dsh --profile headless`（DeepSeek Harness 的无界面单轮模式）。做代码交叉审查（多个 AI 各自独立看
一遍代码抓 bug，再汇总比对）时经常要用到，下面是已经验证过能跑通的调用方式，直接抄，不用再逐个
摸索参数。

## grok：只读、单轮、不卡权限提示

```bash
grok --prompt-file "<绝对路径的提示词文件>" \
     --effort xhigh \
     --disallowed-tools "Edit,Write,NotebookEdit,Bash" \
     --permission-mode bypassPermissions \
     --cwd "<项目绝对路径>" \
     > "<输出文件>" 2>&1
```

- `--prompt-file` 比 `-p "一长串带换行的中文"` 稳，避免 shell 转义/换行的坑；提示词较长（几十行）时
  尤其推荐先写成文件。也可以用 `-p "..."` 做简短的冒烟测试。
- **只读的关键是 `--disallowed-tools` 里显式砍掉 `Edit`/`Write`/`NotebookEdit`/`Bash`**——只留
  Read/Grep/Glob 之类天生只读的工具，这样即使 grok 想改文件也没有能改的工具，不用依赖它"自觉不改"。
- **`--permission-mode bypassPermissions` 是防卡死的关键**：不加这个，即使工具本身是只读的
  （比如 Read），grok 在无人值守的后台运行时仍可能停下来等审批，而没人给它审批，进程就会挂着不动。
  bypassPermissions 让剩下的（已经被 disallowed-tools 限制成只读的）工具集直接放行，两者搭配
  ="真正只读 + 不会挂起"。
- `--effort xhigh` 是推理强度，深度审查用这个；快速冒烟测试可以用 `--effort low` 省时间。
- 想并行开多个（不同模块分工、或同一模块开几份交叉验证）：直接多次调用这条命令，各自不同的
  `--prompt-file`/输出文件路径，用 Bash 工具的 `run_in_background: true` 起后台任务，然后用
  `ScheduleWakeup` 轮询或等任务通知。grok 账号额度大，可以放心开多个。

## dsh：只读模式 + 单轮任务

```bash
DSH_PERMISSION_MODE=read-only dsh --profile headless "<任务文本，可以是 $(cat 提示词文件) 展开的长文本>" \
    > "<输出文件>" 2>&1
```

- `dsh --profile headless` 本身就是"回答一个任务、打印最终结果、退出"的单轮无界面模式，不需要额外
  参数控制这一点。
- **只读靠环境变量 `DSH_PERMISSION_MODE=read-only`**（dsh 内部会把这个值映射到它自己的
  sandbox=read-only + approval=ask 预设）。用 `dsh --profile headless --dump-config` 能看到这套
  映射关系（`dsh-sandbox-policy`/`dsh-permission-presets` 两个内部插件）。
- headless 模式下实测**不会卡在审批提示上**（不像最初担心的那样，read-only + headless 这个组合
  跑起来是干净的单轮返回，没有观察到挂起）。
- 任务文本没有 `--prompt-file` 这种参数，只能拼进命令行参数里；提示词是多行长文本时用
  `"$(cat 文件路径)"` 展开成一个参数传进去，比手写转义可靠。
- dsh 额度/速度不如 grok，多路审查时开的数量按实际情况来，不需要像 grok 那样大量并行。

## 通用建议

- 两边都先跑一个几秒钟的冒烟测试（比如 `-p "只回复两个字：测试成功"` / 同样传给 dsh），确认
  只读参数生效、不会挂起，再放真正的长任务进去——尤其是刚换了新提示词模板或新参数组合的时候。
- 长任务（比如 `--effort xhigh` 审查一个中等大小模块）跑几分钟到十几分钟很正常，用 Bash 工具的
  `run_in_background` 起，配合 `ScheduleWakeup`（1200s 量级的间隔）轮询，不要用短间隔死等。
