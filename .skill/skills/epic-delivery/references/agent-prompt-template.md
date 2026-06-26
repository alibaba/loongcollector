# Agent 任务提示词模板

> 用途：阶段 2 接单时复制并替换占位符。**一个提示词只对应一个子 Issue**。
> 编排 Agent 并行派发时，每个子 Agent 各持一份；子 Agent 推到 draft PR / ReadyToMerge 后**直接返回结果，不等人工反馈**。
> 该提示词应让 Agent 在范围锁内独立完成"开发 → 验收 → 自检 → 开 PR → 处理意见到可合并"。

```text
Preflight（失败则停手，打 needs-human）：
- gh auth status
- git rev-parse --is-inside-work-tree
- ssh -T git@github.com（push 用 SSH 时）

目标：完成 Issue #<n>（Discussion #<d> · 步骤 <步骤号>）

范围（严格锁定）：
- 允许改：<目录/文件清单>
- 禁止改：<目录清单>

约束：
- <项目级约束，如不改默认配置项 X、保持向后兼容>
- 构建与 C++ UT 必须遵循 .claude/skills/compile/SKILL.md（见下）
- 提交信息遵循 .claude/skills/commit/SKILL.md

开发：
1. 先读 .skill/skills/project-knowledge/SKILL.md 建立架构认知
2. 在范围内实现，配套单测 / E2E

验收（进入自检前必须全部通过；禁止写「待 CI」代替本地验收）：
- C++ core 改动：
  - git submodule update --init core/_thirdparty/coolbpf
  - make core PATH_IN_DOCKER=$(pwd)   # 无本机 cmake 时用 Docker，见 compile skill
  - ./scripts/run_core_ut.sh --gtest_filter='<TestSuite>.*' unittest/<dir>/<binary>
- Go 插件：<make plugin_local && go test ./plugins/...>
- 其它：<E2E case 名>

自检（开 PR 前）：
- 按 .skill/skills/review-standards/SKILL.md 逐项自查
- 按 .skill/skills/security-check/SKILL.md 检查密钥 / 合规
- 改动较大时按 .skill/skills/code-review/SKILL.md 走完整评审并落盘

完成动作：
- 用 .claude/skills/commit 写 commit
- 先开 draft PR，body 含 Closes #<n> 与 Test plan（须含实际 PASS 命令输出）
- PR 首评末尾带 footer：`[epic-delivery]` from=agent role=self-review action=none
- 处理 CI 与人工评论直到可合并；人工评论无需 footer，Agent 回复必须带 footer
- 在 Epic Issue 评论 PR 链接（或回复编排 Agent 汇总）
- 推到 ReadyToMerge 后停止；**不要 merge，不要等人工确认再继续**

分支与推送（必遵，避免推错远端 / 污染主仓）：
- 新建远端分支一律建在**个人 fork**（origin 指向 fork），由 fork 向主仓提 PR；不要直接在主仓建分支。
- 推送前**二次确认目标远端**，不要想当然 git push origin：
  - 已有 PR 的分支：gh pr view <pr> --json headRepositoryOwner,headRefName 查 PR head 实际所在 repo，推到该远端（历史遗留：分支若已建在主仓，则后续仍推主仓远端）。
  - git remote -v 区分 fork 与主仓；git rev-parse --abbrev-ref @{u} 确认上游指向正确。
  - push 后用 gh pr view <pr> --json headRefOid 核对远端 head == 本地 HEAD，确认推送真正生效。

回复评论（必遵，避免正文变成字面路径）：
- 用 scripts/epic/epic.sh reply --pr <n> --comment-id <id> --body-file <f>、gh pr comment --body-file <f> 或 gh api ... -F body=@<f>。
- 禁止 gh api -f body=@<f>：-f 把 @路径 当字面字符串原样发送，正文会变成路径而非内容；只有 -F 才会读取文件内容。

禁止：
- 不要 merge、不要 approve、不要 force-push、不要改 CI workflow
- 不要用「本地无 cmake / 待 CI 验证」跳过 compile skill
- 范围扩大或需架构决策时停手，打 needs-human 并 @维护者账号
```
