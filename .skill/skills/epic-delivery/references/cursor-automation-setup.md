# Epic Delivery 事件唤醒（Cloud + 本地）

> **Cursor Automations 只能在云端运行**，不能在本机“离线执行 Automation”。
> 若 `alibaba/loongcollector` 上无法创建 Cloud Agent，用 **本地 Desktop Agent + wake-local.sh** 作为等价唤醒路径。

## 模式对比

| 模式 | 运行位置 | 适用 |
|------|----------|------|
| **Cursor Automation** | Cursor 云端 VM | org GitHub 集成已打通 |
| **本地 Desktop Agent** | 本机 IDE | org 集成未就绪 / 默认推荐 |
| **wake-local.sh + cron** | 本机 shell 轮询 | 半自动：发现 pending 后粘贴提示词 |
| **Cursor CLI + GHA** | 自有 runner | 需 Enterprise API key，仍要 GitHub App |

## 为什么 `alibaba/loongcollector` 可能建不了 Cloud Agent

常见原因（与 YAML 配置无关）：

1. **GitHub org 未批准 Cursor OAuth**：`alibaba` 若限制第三方应用，需 org owner 在 GitHub 设置里 **Grant** Cursor GitHub Extension（不仅安装 App）。
2. **团队级 vs 个人级集成**：Automations 需要 [Dashboard → Integrations](https://cursor.com/dashboard/integrations) 的**团队** GitHub 连接；仅个人 OAuth 时 org 仓可能选不到。
3. **Protected Git Scope**：`alibaba` namespace 若被其他 Cursor 团队 claim，当前团队无法对该仓跑 Cloud Agent。
4. **IP allow list**：GitHub org 启用 IP 限制时需允许 GitHub Apps 或加入 Cursor egress IP。
5. **Repo picker 同步 bug**：App 已装但列表为空 → Dashboard **Disconnect / Reconnect** GitHub。
6. **Fork PR 不支持**：Automation 的 PR comment 触发**不支持 fork 来的 PR**；Epic 分支须在 **upstream**（`alibaba/loongcollector`）上。

文档：[GitHub 集成](https://cursor.com/docs/integrations/github.md) · [Cloud Agents](https://cursor.com/docs/cloud-agent.md) · [Automations](https://cursor.com/docs/cloud-agent/automations.md)

## 本地唤醒（推荐 fallback）

### 1. 人工 PR 评论后

人工**正常评论**即可（无需 footer）。在 Cursor Desktop 新开 Agent 对话，或运行：

```bash
scripts/epic/wake-local.sh --repo alibaba/loongcollector --pr 2619
```

脚本会 Triage 并输出可粘贴的提示词。

### 2. 定时轮询（可选）

```bash
# 每 15 分钟检查 Epic 下 open PR
*/15 * * * * cd /apsara/workspace/loongcollector && scripts/epic/wake-local.sh --repo alibaba/loongcollector --epic 2595 >> /tmp/epic-wake.log 2>&1
```

有 `ACTION REQUIRED` 时，将输出的提示词粘贴到 Desktop Agent。

### 3. Triage 规则摘要

- **Agent 评论**：须带 `` `[epic-delivery] from=agent` ``；`action=none` 不触发处理。
- **无标识评论**：视为**人工意见**，默认需 Agent 处理。
- Automation 场景用 `--latest-only` 只看触发时的最新评论。

## Cloud Automation（集成就绪后）

草稿：`.skill/skills/epic-delivery/references/cursor-automation-draft.yaml`

### 导入

1. Cursor → **Automations** → **New**
2. Trigger：**Comment added on pull request** → `alibaba/loongcollector`
3. Tools：**Comment on PRs**
4. Instructions：复制 draft 中 `prompts`（含 `--latest-only` triage）
5. Git config：repo `alibaba/loongcollector`，branch `main`

或在 **Agents Window** 说：「用 epic-delivery 草稿打开 Automation 编辑器」。

### Cloud Agent 行为

| 步骤 | 动作 |
|------|------|
| 1 | `triage-pr-feedback.sh --pr <n> --latest-only` |
| 2 | 最新评论为 Agent `action=none` → 摘要后退出 |
| 3 | 最新评论无 agent 标识 → 阶段 6 AddressFeedback |
| 4 | C++ 验收遵循 `.claude/skills/compile/SKILL.md` |
| 5 | Agent 回复带 `from=agent` footer |

## 故障排查

| 现象 | 处理 |
|------|------|
| 无法创建 Cloud Agent on org repo | 走**本地 wake-local**；联系 org admin 批 OAuth + 装 App |
| Agent 重复处理自检 comment | Agent 漏打 `from=agent action=none` |
| Agent 忽略人工 comment | 检查是否用了 `--latest-only` 且最新已是 agent 回复 |
| fork PR 不触发 Automation | 分支 push 到 upstream 再开 PR |

## 可选增强

- Label `agent-process-feedback` + Label 触发 Automation
- Cursor CLI in GitHub Actions（[文档](https://cursor.com/docs/cli/github-actions.md)）在自有 runner 调 `agent -p`
