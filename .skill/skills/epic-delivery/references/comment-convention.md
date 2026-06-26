# Epic Delivery 评论标识规范

> **原则**：**只要求 Agent 发评带标识**；人工评论**无需**任何格式。
> 编排 Agent 唤醒后用 `gh` 读评论，结合 footer 区分 Agent 信息与待处理人工意见。

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
| `from` | 固定 `agent` | Agent 消息 |
| `role` | `self-review` / `feedback-handler` / `orchestrator` | 用途分类 |
| `action` | `none` | 信息性 |
| | `required` | 少见，显式请求跟进 |
| | `fyi` | 仅供参考 |

### 示例

**自检**：

```markdown
**自检**：Critical 0；范围锁遵守。

---
`[epic-delivery]` from=agent role=self-review action=none
```

**处理完人工意见**：

```markdown
已按 review 修改。

---
`[epic-delivery]` from=agent role=feedback-handler action=none
```

## 人工评论（无格式要求）

人工正常写评论即可，不必加 footer。编排 Agent 用 `gh pr view` 拉取后，**无 `from=agent` 标识**的评论视为待处理人工意见。

## 回复位置（必遵）

处理人工 PR 意见时，**禁止**在 PR 顶层另开一条新评论来「汇总处理结果」（避免像单独发「已按意见修改 / Test plan 更新」那样与人工评论平级）。

| 评论类型 | 正确回复方式 |
|----------|--------------|
| **Review 行评**（Files changed 线程） | 在该 thread 内回复：`scripts/epic/epic.sh reply --pr <n> --comment-id <review_comment_id> --body-file …` 或 `POST .../pulls/{pr}/comments/{id}/replies` |
| **Conversation 评论**（PR 讨论区） | `epic.sh reply` 优先 `issues/comments/{id}/replies`；若 API 不可用，自动 **Quote reply**（引用原文 `>`），**禁止**无引用的顶层「已按意见修改 / Test plan 更新」类汇总评 |

回复正文末尾仍须带 Agent footer（`role=feedback-handler action=none`）。处理完成后用 `scripts/epic/epic.sh events --epic <n> mark-handled <comment_id>` 标记，**不要**再额外发一条顶层 PR 评论重复说明。

**工具用法（必遵，避免正文变成字面路径）**：回复正文一律用 `epic.sh reply --body-file <f>`、`gh pr comment --body-file <f>` 或 `gh api … -F body=@<f>`；**禁止** `gh api -f body=@<f>`——`-f` 把 `@路径` 当字面字符串原样发送，正文会变成路径而非内容（只有 `-F` 才会读取文件）。

## 在哪里回答评论里的问题（必遵）

评论里的**提问 / 质疑 / 选型**（如「这段代码未来怎么用？」「为什么这么实现？」），**回应必须回到对应的 GitHub 评论 thread**，与维护者在 GitHub 上闭环。

- **禁止**把评论里的问题搬到本地 Agent 对话里向人确认（提问发生在 GitHub，回答也应在 GitHub）。
- 需要维护者拍板的方向选择：在 GitHub 评论里**给出选项 + 推荐方案 + 依据**并 `@`（at 仓库实际维护者的 GitHub 账号，如 `@Takuka0311`；不要写 `@maintainer` 这类占位，那会误 at 到真实的同名用户），靠下一轮轮询捕获其答复，**不在本地阻塞确认**。
- 仅当问题与本 Epic 无关、或属于「本地环境 / 凭证 / 工具」类编排自身障碍时，才在本地与发起人沟通。

## 编排 Agent 判读

| 评论特征 | 处理 |
|----------|------|
| `from=agent` + `action=none` | 跳过 |
| `from=agent` + `action=fyi` | 跳过 |
| 无 agent 标识 | 人工意见，进入 AddressFeedback |
