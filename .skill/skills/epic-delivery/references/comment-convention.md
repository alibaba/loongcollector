# Epic Delivery 评论标识规范

> **原则**：**只要求 Agent 发评带标识**；人工评论**无需**任何格式，Triage 默认视为「人工 + 需处理」。
> 机器侧通过 `` `[epic-delivery] from=agent` `` 识别 Agent 消息；无标识的一律当人工意见。

## Agent 标识（必遵）

Agent 在 Issue / PR 评论**末尾**附加 footer（与正文空一行）：

```markdown
正文……

---
`[epic-delivery]` from=agent role=<role> action=<none|required|fyi>
```

### 字段

| 字段 | Agent 常用取值 | 含义 |
|------|----------------|------|
| `from` | 固定 `agent` | 机器可识别的 Agent 消息 |
| `role` | `self-review` / `feedback-handler` / `orchestrator` | 用途分类 |
| `action` | `none` | 信息性，**不**触发 AddressFeedback |
| | `required` | Agent 显式请求编排跟进（少见） |
| | `fyi` | 仅供参考 |

### Agent 示例

**自检（开 PR 后首评）**：

```markdown
**自检**：Critical 0；范围锁遵守。

---
`[epic-delivery]` from=agent role=self-review action=none
```

**处理完人工意见**：

```markdown
已合并矩阵到 overview.md。

---
`[epic-delivery]` from=agent role=feedback-handler action=none
```

发评辅助（Agent 专用）：

```bash
scripts/epic/gh-comment.sh pr 2619 --from agent --role self-review --action none <<'EOF'
**自检**：……
EOF
```

## 人工评论（无格式要求）

人工在 PR / Issue **正常写评论即可**，不必加 footer。

Triage 规则：

| 评论特征 | 判定 | 是否触发 AddressFeedback |
|----------|------|--------------------------|
| 含 `from=agent` + `action=none` | Agent 信息 | **否** |
| 含 `from=agent` + `action=fyi` | Agent 参考 | **否** |
| 含 `from=agent` + `action=required` | Agent 请求跟进 | 是 |
| **无** `[epic-delivery]` 标识 | **人工** | **是**（默认） |

> 若 Agent 漏打标识，其**自检**类评论可能仍被启发式跳过；**非自检**的 Agent 消息会被误判为人工——Agent 必须带 footer。

## 启发式（仅无标识时）

| 无标识评论特征 | 判定 |
|----------------|------|
| 含 `**自检**` 或 `[epic-delivery triage]` | 视为 Agent 信息，跳过 |
| 其它 | **人工**，需处理 |

## Triage 脚本

```bash
# 列出 PR 上所有待处理人工意见
scripts/epic/triage-pr-feedback.sh --repo alibaba/loongcollector --pr 2619

# Automation / 事件唤醒：仅看最新一条评论
scripts/epic/triage-pr-feedback.sh --repo alibaba/loongcollector --pr 2619 --latest-only
```

## Cursor Automation 联动

PR `COMMENTED` 触发时：

1. `triage-pr-feedback.sh --pr <n> --latest-only`
2. 最新评论为 Agent `action=none` → 发 orchestrator 摘要后退出
3. 最新评论无标识（人工）→ 进入 epic-delivery 阶段 6

详见 `references/cursor-automation-setup.md`（含**本地唤醒**方案）。
