按 epic-delivery 执行 {{TASK}}（Epic #{{EPIC}}）。

=== Skills（worktree 根目录下 Read，执行前必做）===
{{SKILLS}}

=== 环境 ===
- worktree: {{WORKTREE}}
- 分支: {{BRANCH}}
- push 远端: {{PUSH_REMOTE}}（PR head 所在；push 后 gh pr view {{PR_NUM}} --json headRefOid 核对）
- comment_id: {{COMMENT_ID}}
- 事件 URL: {{URL}}

=== 关联 Issue / PR ===
- Closes Issue: #{{CLOSES_ISSUE}} {{ISSUE_TITLE}}
- PR 标题: {{PR_TITLE}}

=== 范围锁（Issue 正文摘录，不得扩大）===
允许: {{SCOPE_ALLOW}}
禁止: {{SCOPE_DENY}}

=== 验收标准（Issue 摘录）===
{{ACCEPTANCE}}

=== 维护者意见（本轮必须处理）===
{{MAINTAINER_COMMENT}}
{{REVIEW_LOC}}

=== 执行步骤 ===
1. 读上述 Skills；先验证维护者意见是否成立，再改代码
2. 在 worktree/分支完成改动；验收见 testing-standards / compile（禁止「待 CI」代替本地跑通）
3. push 到 {{PUSH_REMOTE}}/{{BRANCH}}
4. 回复维护者（必遵）：
   - 正文套 `.skill/skills/epic-delivery/references/reply-feedback.md.tpl`（含 footer）
   - `./scripts/epic/epic.sh reply --epic {{EPIC}} --pr {{PR_NUM}} --comment-id {{COMMENT_ID}} --body-file reply.md`
   - comment_id 可用 IC_...（脚本会自动解析）；**禁止**手搓 quote-reply 或裸 `gh api issues/comments/IC_...`

=== 回报编排 Agent ===
commit、push 目标、验收命令与结果、是否 ReadyToMerge

禁止：merge；架构争议打 needs-human
