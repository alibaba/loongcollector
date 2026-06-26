按 epic-delivery 执行 {{TASK}}（Epic #{{EPIC}}）。

=== Skills（worktree 根目录下 Read，执行前必做）===
{{SKILLS}}

=== Preflight（失败则停手，打 needs-human）===
- gh auth status
- git rev-parse --is-inside-work-tree
- ssh -T git@github.com（push 用 SSH 时）

=== 环境 ===
- worktree: {{WORKTREE}}
- 若无 worktree: {{WT_NEW_CMD}}
- 分支: {{BRANCH_HINT}}（push 到 origin/fork）
- Issue URL: {{ISSUE_URL}}

=== Issue ===
- #{{ISSUE_NUM}} {{ISSUE_TITLE}}
{{BLOCKED_LINE}}

=== 范围锁（Issue 正文摘录，不得扩大）===
允许: {{SCOPE_ALLOW}}
禁止: {{SCOPE_DENY}}

=== 验收标准（Issue 摘录）===
{{ACCEPTANCE}}
{{PREREQ_BLOCK}}=== 执行步骤 ===
1. 读上述 Skills；Preflight 通过后进入 worktree（无则 wt new）
2. Develop → SelfReview → 开 draft PR（Closes #{{ISSUE_NUM}}；body 含 Test plan）
3. Issue/Epic 评论 PR 链接 + 自检摘要
4. 处理 CI/评论直到 ReadyToMerge；禁止 merge

=== 回报编排 Agent ===
Issue 号、worktree 路径、分支名、PR URL、验收命令与结果、ReadyToMerge / Blocked

禁止：merge；范围扩大打 needs-human；禁止「待 CI」代替本地验收
