# 执行 Agent 派发模板（Develop / AddressFeedback）

> **编排 Agent 派 Task 时：子 Agent 无会话记忆，必须整段复制 `dispatch_prompt`。**  
> 正文由 `scripts/epic/dispatch-enrich.sh` 渲染 skill 内 MD 模版生成：  
> `references/dispatch-address-feedback.md.tpl`、`references/dispatch-develop.md.tpl`

## 子 Agent 为何需要显式上下文

| 编排 Agent 有 | 子 Agent **没有** |
|---------------|-------------------|
| 本会话 Epic 进度、历史决策 | 仅 Task 文本 |
| 已读 skill | 需路径列表 |
| worktree / 分支 / push 远端 | 需写清或会 push 错仓 |
| Issue 范围锁、验收标准 | 需从 Issue 摘录 |
| 维护者完整评论 | 需原文或 URL |
| merge 前置交付摘要 | 需 `--prerequisite-*` 或 Issue 评论链接 |

Skill 在 **worktree 内** `.skill/skills/`（与各分支 checkout 一致）。

---

## 三种派发路径（统一 `dispatch_prompt`）

| 场景 | 命令 | 复制字段 |
|------|------|----------|
| **AddressFeedback**（PR/Issue 评论） | `./scripts/epic/epic.sh inbox --epic <N> --json` | 每条 `dispatch_prompt` |
| 单条 pending | `./scripts/epic/epic.sh dispatch-prompt --epic <N> <comment_id>` | stdout 全文 |
| **Develop**（新 Issue / 并行开工） | `./scripts/epic/epic.sh dispatch-prompt --epic <N> --issue <M>` | stdout 全文 |
| **merge 解锁** | `./scripts/epic/epic.sh merge-followup --epic <N> --pr <P> --json` | `unlocked[].dispatch_prompt` |

`inbox --json` 另含结构化字段：`skills[]`、`context{}`（编排 Agent 可读，**派发仍复制 `dispatch_prompt`**）。

---

## `dispatch_prompt` 必含块（脚本自动生成）

| 块 | Develop | AddressFeedback |
|----|---------|-----------------|
| Skills 路径列表 | ✓ | ✓ |
| Preflight（gh / git / ssh） | ✓ | — |
| worktree / 分支 / push 远端 | ✓ | ✓ |
| Issue 范围锁 + 验收标准 | ✓ | ✓ |
| 维护者意见原文 | — | ✓ |
| review 行评 path:line | — | ✓（如有） |
| 前置依赖摘要 | ✓（merge 解锁时） | — |
| reply / mark-handled 步骤 | — | ✓ |
| 回报编排 Agent 格式 | ✓ | ✓ |

---

## 编排 Agent 标准流程

### AddressFeedback

```bash
./scripts/epic/epic.sh inbox --epic 2595 --json
# 或：
./scripts/epic/epic.sh dispatch-prompt --epic 2595 IC_kwD...
```

1. 读 JSON 的 **`dispatch_prompt`**（不要重写）
2. `Task` 派执行 Agent：**粘贴完整 `dispatch_prompt`**
3. 子 Agent 完成后 `mark-handled <comment_id>`

### 并行 Develop（无 Blocked by）

```bash
./scripts/epic/epic.sh dispatch-prompt --epic 2595 --issue 2598
```

对每个 `agent-ready` 且无 `Blocked by` 的 Issue 各派一条 Task。

### merge-followup 解锁

```bash
./scripts/epic/epic.sh merge-followup --epic 2595 --pr 2619 --json
./scripts/epic/epic.sh events --epic 2595 mark-handled prstate-2619-MERGED
```

对 `unlocked[]` **每条**复制 `dispatch_prompt` 立即派 Task（不要只发 unlock 评论）。

---

## 禁止

- 只写「处理 PR #2623 评论」而不带 Skills / worktree / 维护者原文
- 假设子 Agent 记得编排会话里的历史决策
- 省略 push 远端（易 push 到 fork/upstream 错误）
- merge 后只 unlock 不派工
