# GitHub 模板（Epic Issue / 子 Issue / PR）

> 用途：阶段 1 拆解与阶段 5 开 PR 时套用。替换 `<...>` 占位符。
> **正文一律用真实换行**（写入 `--body-file`），不要用 bash 双引号里的 `\n`。

---

## Epic Issue

标题：`[Epic] <目标名>（Discussion #<n>）`

创建：

```bash
gh issue create --repo <owner>/<repo> \
  --title "[Epic] <目标名>（Discussion #<n>）" \
  --body-file epic-body.md \
  --label epic
```

正文（`epic-body.md`）：

```markdown
## 目标
关联 Discussion: #<n>
一句话说明本 Epic 的终态。

## 步骤 Checklist（进度唯一事实来源）
- [ ] #<子Issue-A1> A1 <一句话>
- [ ] #<子Issue-B1> B1 <一句话>

## 阶段与依赖
见 Discussion #<n> 的"分阶段依赖"。

## 子 Issue
子任务通过 GitHub Sub-issues 面板挂载在本 Epic 下（`gh issue edit <epic> --add-sub-issue <n>`）。
```

> Checklist 与 Sub-issues 面板互补：前者便于 Agent 扫一眼进度，后者是 GitHub 原生层级关系。

---

## 子 Issue

标题：`[<epic>-<步骤>] <一句话>`（如 `[1928-B1] add PassThroughEvents helper`）

创建（**必须 `--parent`**）：

```bash
gh issue create --repo <owner>/<repo> \
  --title "[1928-B1] add PassThroughEvents helper" \
  --body-file sub-issue-body.md \
  --label epic,track/B,phase/1,agent-ready \
  --parent <EPIC_NUMBER>
```

正文（`sub-issue-body.md`）——注意验收标准与 Agent 提示词各占**独立行**：

```markdown
## 背景
关联：Discussion #<n> · 步骤 <B1>；Epic：#<epic>
<为什么做这一步>

## 验收标准
- [ ] <构建/测试命令> 通过
- [ ] 不破坏 <约束，如默认配置项 X>
- [ ] PR 描述含影响面 / 回滚方式

## 范围锁
- 允许改：`pkg/pipeline`、`pkg/helper` 及相关单测
- 禁止改：`plugins/`、`core/` 等其它目录

## 依赖
- Blocked by: #<x>（无则写 —）
- Blocks: #<y>（无则写 —）

## Agent 提示词

见 `references/agent-prompt-template.md`，按本 Issue 填好范围与验收命令（提示词内每行一条，不要用 `\n` 拼接）。

标签：`epic`、`track/*`、`phase/*`；就绪可接单加 `agent-ready`。

### 常见错误

| 错误写法 | 现象 | 正确写法 |
|----------|------|----------|
| bash 双引号 `"line1\nline2"` | Issue 里显示字面量 `\n` | `--body-file` 或多行 heredoc |
| 只正文写 Epic: #2595 | 无 Sub-issues 面板 | `--parent` 或 `--add-sub-issue` |

---

## PR Body

PR 说明须**足够详细**：让读者不看 Issue 也能理解上下文，并明白改动对 Epic 整体目标的价值。起步开 **draft PR**。

```markdown
Closes #<issue>

## 对应步骤
[Discussion #<n>](<disc-url>) · **主线 <X> · 步骤 <Xn>** · [Epic #<epic>](<epic-url>)

## 背景与上下文
（2–4 句：当前痛点、本步骤在依赖图中的位置、与前后步骤的关系）

## 对整体目标的价值

| 演进线 | 本 PR 贡献 |
|--------|-----------|
| **A** | …（无关则写「无直接依赖」） |
| **B** | … |
| **C** | … |
| **D** | … |

## 改动
- <要点 1>
- <要点 2>

**行为变更**：有 / 无（若无，明确写「纯新增 API / 仅文档」等）

## Test plan
- [x] `<命令>` — PASS / FAIL
- [x] 自检：Critical N / Suggestion N

## 影响面 / 回滚
- **影响面**：…
- **回滚**：revert 本 PR 即可 / …
- **约束遵守**：未改默认 StructureType；未越 scope

## 合并顺序
无 stacked / 在 #<前序 PR> 之后
```

---

## PR / Issue 评论 footer（仅 Agent 必附）

**人工评论无格式要求。** Agent 评论末尾附加 footer，见 `comment-convention.md`：

```markdown
---
`[epic-delivery]` from=agent role=self-review action=none
```

Agent 发评：`scripts/epic/gh-comment.sh pr <n> --from agent --role self-review --action none`
