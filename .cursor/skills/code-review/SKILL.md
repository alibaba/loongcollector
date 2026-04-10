---
name: code-review
description: 在进行 Code Review 时，使用这个技能对 LoongCollector 变更进行安全导向、架构一致性优先的深度代码评审。
metadata:
  requires:
    bins:
      - python3
      - git
      - gh
---
# Code Review Agent Skill

你是 LoongCollector 项目的高级代码审查助手。你的核心目标是发现真实缺陷、行为回归和风险点，而不是给出泛泛建议。

为避免假阳性，必须遵守：

- 分析问题时必须包含充分上下文，不能只看局部 diff 就下结论。
- 结论必须基于实际读取到的代码与变更，不允许基于记忆或猜测。
- 先理解作者意图和端到端流程，再给出问题判断。
- 遵循以下执行步骤，以实现代码修改后可以针对增量 Review，检查既有评审的修复情况。

## TOC

- [Preflight（确保依赖工具存在）](#preflight确保依赖工具存在)
- [Local Branch Sync（确保代码新鲜）](#local-branch-sync确保代码新鲜)
- [Review Plan（开始前规划，避免遗漏）](#review-plan开始前规划避免遗漏)
- [脚本失败降级策略](#脚本失败降级策略)
- [Phase 1: Review Workspace & Incremental State（评审工作区与增量状态）](#phase-1-review-workspace--incremental-state评审工作区与增量状态)
- [Phase 2: Context Building（全局认知）](#phase-2-context-building全局认知)
- [Phase 3: Intent Analysis（意图理解）](#phase-3-intent-analysis意图理解)
- [牢记评估标准（无需输出）](#牢记评估标准无需输出)
- [Phase 4: Sub-agent Review（专项检查）](#phase-4-sub-agent-review专项检查)
- [Phase 5: Final Report（最终输出）](#phase-5-final-report最终输出)

## Preflight（确保依赖工具存在）

在进入 Phase 1 前，必须先执行以下命令并全部通过：

- `python3 --version`
- `git rev-parse --is-inside-work-tree`
- `gh auth status`

若任一命令失败，必须停止后续评审步骤，并按 `references/failure-playbook.md` 修复后重试。

## Local Branch Sync（确保代码新鲜）

当复用本地 PR 分支做评审时，请在正式评审前先同步一次代码，避免使用过期工作副本：

1. 读取远程 PR 当前 `headRefOid`（或分支当前 `HEAD` SHA）。
2. 对应本地分支执行同步（如 `git fetch` + `git pull --ff-only` 或等价流程）。
3. 在 `final-report.md` 顶部记录本轮评审使用的 `head` SHA，便于追溯。

## Review Plan（开始前规划，避免遗漏）

在进入 Phase 1 细节步骤前，先在评审目录生成并维护 `review-plan.md`，用于“逐步执行 + 勾选校验”：

1. 文件路径：
   - PR：`code-review/pr-<number>/review-plan.md`
   - 分支：`code-review/branch-<name>/review-plan.md`
2. 至少包含：
   - 本轮评审对象（PR/分支、base/head SHA）
   - 本轮待办清单（checkbox），按“**大项 + 子项**”拆分
   - 当前阶段标记（`in_progress`）
   - 阻塞项与降级记录（若有）
3. 执行要求：
   - 每完成一个步骤，必须同步勾选；
   - 若中断或切换策略（如 `incremental -> full`），必须先更新计划再继续。
   - 不允许只写 Phase 名称而不拆子项（例如“Phase 1”必须细分到拉评论、更新状态、映射决策等子项）。
4. 模板使用：
   - `references/review-plan.template.md` 仅提供骨架；
   - agent 必须根据本轮实际情况自行填写大项与子项。

## 脚本失败降级策略

若执行脚本报错，允许进入降级评审模式继续完成代码评审，但必须执行以下动作用于持续优化 skill：

- 在 `code-review/<target>/script-failures.md` 记录失败信息（脚本名、命令、错误摘要、触发时间、回退策略）。
- 评审继续时一律切换到 `full` 全量评审，并人工核对关键状态文件。
- 在 `final-report.md` 增加 “Script Failure Feedback” 小节，说明失败影响范围与人工补偿动作。
- 将失败信息反馈到技能维护通道（可用时使用 `mcp-feedback-enhanced`，不可用时至少落盘到 `script-failures.md` 供后续回收）。

## Phase 1: Review Workspace & Incremental State（评审工作区与增量状态）

开始评审前，先初始化或复用仓库根目录下的评审工作区：

- PR 评审目录：`code-review/pr-<number>/`
- 分支评审目录：`code-review/branch-<name>/`
- 目录不存在时必须创建，且保留历史评审轮次

该目录至少包含以下文件：

- `meta.json`：评审对象与基线元数据（repo、base/head、review 时间、策略参数）
- `review-plan.md`：本轮执行计划与勾选进度（先计划再执行）
- `reviewed_commits.json`：已评审 commit 集合与映射记录
- `intent-architecture-notes.md`：代码理解文档（Phase 3）
- `final-report.md`：最终报告（Phase 5）
- `comments/review-comments.json`：PR review comments 原始快照（仅此来源）
- `comments/comment-status.json`：评论状态判定结果（流程状态 + 技术状态）

输入门禁：

- 首次运行：
  - 允许上述文件不存在；
  - 必须先执行初始化脚本生成最小文件骨架，再继续后续步骤。
- 非首次运行：
  - 关键输入文件必须存在且 schema 合法；
  - 若不合法，必须按 `references/failure-playbook.md` 执行“全量重建/重抓取”恢复流程，不允许手工拼接 JSON 继续运行。

模板与脚本目录（必须使用）：

- JSON 模板：`/.cursor/skills/code-review/references/`
- 流程脚本：`/.cursor/skills/code-review/scripts/`

执行步骤（必须按顺序）：

1. 初始化评审目录与基础文件：
   - PR：`python3 .cursor/skills/code-review/scripts/init_review_workspace.py --repo-root <repo> --target-type pr --target-id <pr> --base-ref <baseRef> --head-ref <headRef> --base-sha <baseSha> --head-sha <headSha>`
   - 分支：`python3 .cursor/skills/code-review/scripts/init_review_workspace.py --repo-root <repo> --target-type branch --target-id <branchName> --base-ref <baseRef> --head-ref <headRef> --base-sha <baseSha> --head-sha <headSha>`
2. 生成/更新 `review-plan.md`（可基于 `references/review-plan.template.md` 骨架，但必须补齐本轮大项/子项），并将当前阶段标记为 `Phase 1 in_progress`。
3. 拉取 review comments 到 `comments/review-comments.json`：
   - PR 评审：必须运行 `python3 .cursor/skills/code-review/scripts/fetch_review_comments.py --repo-root <repo> --target-type pr --target-id <pr>`，仅 `PR review comments`
   - 分支评审：可为空，或导入分支评审评论快照
   - `review-comments.json` 必须是标准对象结构（根对象含 `comments` 数组，元素含 `comment_id/path/line/side/body`）；若不满足，视为上游脚本错误，必须先修正上游脚本。
   - 评论项必须包含 `thread_resolved` 布尔字段；流程状态仅由该字段决定（`true -> resolved`，`false -> open`）。
   - `snapshot/` 必须保留源码相对路径层级，禁止平铺文件名。示例：`snapshot/round-2/files/core/ebpf/protocol/redis/RedisParser.cpp`。若出现平铺结果，视为快照脚本错误或中途中断，必须重跑修正。
4. 生成/更新评论状态文件：
   - PR：`python3 .cursor/skills/code-review/scripts/update_comment_status.py --repo-root <repo> --target-type pr --target-id <pr>`
   - 分支：`python3 .cursor/skills/code-review/scripts/update_comment_status.py --repo-root <repo> --target-type branch --target-id <branchName>`
   - 说明：这一步只同步结构与流程状态（`status_flow`）并保留历史 `status_tech`，不会自动做代码复核判定。
5. 生成双维状态 Markdown 报告（表格）：
   - PR：`python3 .cursor/skills/code-review/scripts/generate_comment_status_report.py --repo-root <repo> --target-type pr --target-id <pr>`
   - 分支：`python3 .cursor/skills/code-review/scripts/generate_comment_status_report.py --repo-root <repo> --target-type branch --target-id <branchName>`
   - 输出文件固定为：`comments/comment-status.md`（列：评论时间、文件、行号、作者、评论、流程状态、技术状态）
6. 计算增量映射与回退建议（`--base` 与 `--head` 必须传 commit SHA）：
   - PR：`python3 .cursor/skills/code-review/scripts/incremental_review_mapper.py --repo-root <repo> --target-type pr --target-id <pr> --base <baseSha> --head <headSha> --review-round <n>`
   - 分支：`python3 .cursor/skills/code-review/scripts/incremental_review_mapper.py --repo-root <repo> --target-type branch --target-id <branchName> --base <baseSha> --head <headSha> --review-round <n>`
   - 当 `snapshot/latest.json` 存在时，映射脚本会计算 `snapshot_match_rate`，用于 rebase 冲突调整或 squash 合并后的增量决策辅助。
7. 根据脚本输出中的 `recommendation` 执行：
   - `incremental`：只评审 `need_review_commits`
   - `partial`：优先评审 `need_review_commits`，并补审低置信 hunk
   - `full`：执行全量评审，但必须做历史意见去重

8. 技术状态（`status_tech`）必须逐条复核，不允许猜测：
   - 必读输入（按顺序）：
     1) `comments/review-comments.json`
     2) `comments/comment-status.json`
     3) `reviewed_commits.json`
     4) 当前代码中与 comment `path` 对应文件
     5) `snapshot/` 中同路径历史快照文件（若存在）
   - 逐条处理规则（按 `comment_id`）：
     - 仅允许更新：`status_tech`、`mapped_finding_id`、`notes`
     - `status_tech` 仅可取：`fixed|not-fixed|false-positive|partially-fixed`
     - `notes` 必须写明“判定证据”，至少包含：对比文件、关键代码变化、结论原因
     - 每轮必须优先复核上一轮未终态条目（`not-fixed`、`partially-fixed`）。
   - 人工手动订正（支持）：
     - 若评论作者本人（当前 `gh` 登录账号）在该评论线程回复文本包含 `fixed`，状态同步为 `fixed`。
     - 若回复文本包含 `false-positive`（或 `false positive`），状态同步为 `false-positive`。
     - 手动订正由脚本在更新 `comment-status.json` 时自动吸收，并写入 `notes`。
   - 终态跳过规则（默认开启）：
     - 当前 `status_tech` 为 `fixed` 或 `false-positive` 的条目，本轮默认跳过技术复核。
     - 仅在以下条件触发时重开复核：
       1) 条目 `path` 在本轮 commit 范围内再次发生修改；
       2) 条目 `status_flow` 从 `resolved` 变为非 `resolved`；
       3) 人工显式指定强制复核（按 `comment_id` 列表）。
   - 输出要求：
     - 更新后的 `comments/comment-status.json`
     - 重新生成 `comments/comment-status.md`
9. 本轮评审收尾后，必须生成 snapshot 供下一轮增量决策使用：
   - PR：`python3 .cursor/skills/code-review/scripts/build_snapshot.py --repo-root <repo> --target-type pr --target-id <pr> --base <baseSha> --head <headSha> --review-round <n>`
   - 分支：`python3 .cursor/skills/code-review/scripts/build_snapshot.py --repo-root <repo> --target-type branch --target-id <branchName> --base <baseSha> --head <headSha> --review-round <n>`
   - 产物：`snapshot/round-<n>/files/*`、`snapshot/round-<n>/manifest.json`、`snapshot/latest.json`

状态文件字段约束（必须遵守）：

- `reviewed_commits.json` 记录：
  - `commit_sha`
  - `patch_id`（用于 rebase 后精确映射）
  - `review_round`
  - `reviewed_at`
  - `hunk_fingerprints`（数组）
- `comments/comment-status.json` 记录：
  - `comment_id`
  - `path` / `line` / `side`
  - `body`
  - `snippet`（可读代码片段）
  - `snippet_fingerprint`（规范化片段 hash）
  - `status_flow`（`open|resolved|wont-fix|deferred`）
  - `status_tech`（`fixed|not-fixed|false-positive|partially-fixed`）
  - `mapped_finding_id`

说明：

- `snippet_fingerprint` 定义为“规范化代码片段 + 文件路径 + 评论定位三元组（line/side/comment_id）”的稳定 hash，不能只用行号。
- 允许人工修正 `status_flow` 与 `status_tech`，但不得删除历史记录。

增量评审策略（必须执行）：

1. 优先读取 `reviewed_commits.json`，只评审未覆盖的新变更。
2. 若检测到 rebase/force-push，不可直接判定全量重审，先做映射再决策：
   - L1（高置信）：按 `patch-id` 映射旧 commit -> 新 commit，命中后继承“已评审”状态。
   - L2（中置信）：按 `path + 规范化 hunk 片段 + hunk 上下文` 做指纹匹配，仅补审未命中 hunk。
   - L3（低置信）：命中率低或冲突改写明显时，回退全量评审。
3. 置信度门槛默认：
   - `commit_map_rate >= 90%`：增量通过
   - `hunk_match_rate >= 80%`：局部补审
   - 否则全量回退
4. 即使全量回退，也必须复用历史评论与 finding 去重，避免重复意见。

snapshot 在增量决策中的职责（必须遵守）：

1. `snapshot` 是增量决策辅助依据，不替代 git 主链路（`patch-id`/`hunk`）。
2. rebase 且发生冲突改写时，若 commit/hunk 映射不足，可使用 `snapshot_match_rate` 辅助从 `full` 降到 `partial`。
3. squash 合并导致 commit 边界丢失时，`snapshot_match_rate` 用于判断是否可继续增量评审。
4. 若 `snapshot_match_rate` 不足阈值，仍必须 `full` 全量评审。

## Phase 2: Context Building（全局认知）

开始评审前，必须先完成以下步骤：

1. 读取 `/.cursor/rules/project-knowledge/architecture.md`，建立系统架构和模块职责认知。
2. 读取 `/.cursor/rules/project-knowledge/codebase-map.md`，优先吸收：
   - 公共能力入口（必须复用的 common/helper）
   - 生命周期与资源释放不变量
   - 配置/环境变量约定（兼容大小写、默认值、废弃参数映射）
   - 历史 review 高频问题（作为优先检查清单）
3. 读取并参考以下规范（按变更涉及范围选择）：
   - `/.cursor/skills/selfmonitor/SKILL.md`（自监控与告警相关改动必读）
   - `/.cursor/skills/security-check/SKILL.md`（安全与合规相关改动必读）
   - `/.cursor/skills/compile/SKILL.md`（涉及构建/编译链路时必读）
4. 基于 PR/分支变更列表，读取受影响文件的完整上下文（至少覆盖变更函数、调用方、定义处）。
5. 若改动涉及 pipeline/runner/配置系统，必须先阅读以下代码再下结论：
   - `core/application/Application.cpp`（主循环、配置扫描、退出顺序）
   - `core/collection_pipeline/CollectionPipelineManager.cpp`
   - `core/collection_pipeline/CollectionPipeline.cpp`
   - `core/runner/ProcessorRunner.cpp`
   - `core/runner/FlusherRunner.cpp`
   - `core/config/watcher/PipelineConfigWatcher.cpp`
   - `core/config/OnetimeConfigInfoManager.cpp`
   - `core/file_server/FileServer.cpp`
   - `core/file_server/checkpoint/CheckPointManager.cpp`
   - `core/file_server/checkpoint/CheckpointManagerV2.cpp`（改动涉及 exactly-once 时）
6. 通过 MCP/`gh` 工具拉取评审上下文：
   - PR 描述、提交历史、PR review comments、CI 状态
   - 最近约 10 个相关 PR 的 review 评论（提炼团队偏好）
7. 若可访问 Code 平台历史评论，优先抽样最近已合入 PR 的 review comments（建议>=30条）并做“模式交叉”：
   - 把历史高频问题映射到本次变更文件，标记为“高风险检查项”
   - 若与 `codebase-map` 冲突，以“最新代码事实 + 评论证据”更新结论
8. 若发现历史约束或设计决策冲突，先记录“假设与证据”，后续在报告中显式说明。

## Phase 3: Intent Analysis（意图理解）

完成上下文分析后，必须先产出“理解文档”，再进入问题列表。该文档是给开发者学习和理解代码用的，不能省略。

### Phase 3 输出要求（必须输出文档）

必须输出一个独立文档（建议标题：`Code Review - Intent & Architecture Notes`），至少包含：

- 作者意图：这个 PR/分支要解决什么问题，为什么现在做。
- 端到端流程：从入口到出口，这次变更实际改变了哪些关键路径。
- 影响范围：涉及哪些模块、接口、配置、状态文件、监控指标、告警链路。
- 预期结果验证：改动是否达到目标，并给出证据与推理过程。

### Phase 3 落盘要求（必须写入 code-review 目录）

必须将 Phase 3 文档写入仓库 `code-review/` 目录，禁止只在聊天中输出。

建议路径：

- PR 评审：`code-review/pr-<number>/intent-architecture-notes.md`
- 分支评审：`code-review/branch-<branchName>/intent-architecture-notes.md`（`/` 替换为 `-`）

要求：

- 若目录不存在必须先创建。
- 文档顶部必须包含评审对象元信息（PR号/分支名、commit范围、生成时间）。

### Mermaid 可视化要求（必须至少 2 张图）

该理解文档必须包含 Mermaid 图，用于帮助学习与沟通。按改动内容选择，至少输出以下 2 类中的 2 张：

- 架构图（模块关系 / 依赖边界）
- 流程图（关键执行路径）
- 时序图（组件交互、调用顺序、异步/重试行为）
- 数据结构图（关键状态对象、队列、checkpoint 主从关系）

建议：

- 小改动：至少 2 张图（流程 + 时序）
- 中大型改动：3-4 张图（架构 + 流程 + 时序 + 数据结构）

注意：

- 图必须与当前变更强相关，禁止画与本次 PR 无关的“百科全图”。
- 图中节点命名使用代码中的真实组件/类型名称，避免抽象空词。
- Mermaid 语法请遵循 `/.cursor/skills/mermaid/SKILL.md`。

## 牢记评估标准（无需输出）

对每个变更文件和差异块，按以下 6 组标准检查：

1. 业务与架构：目标达成、职责边界、拓扑与依赖、故障传播。
2. 正确性与安全：边界检查、类型/异常处理、外部输入防御、安全合规。
3. 并发与生命周期：线程/锁/队列正确退出、资源释放、状态恢复。
4. 性能与资源：热路径复杂度、拷贝与分配、容量上限、日志开销。
5. 稳定性与可观测：指标/日志/告警完整性与可定位性。
6. 可维护性、兼容性与文档测试：可读性、向后兼容、文档与测试覆盖。

注意：以上不是“通用建议列表”，而是必须落到每个 sub-agent 的责任范围中执行（见下一节责任矩阵）。

## Phase 4: Sub-agent Review（专项检查）

并行启动专项 sub-agent（建议 3-4 个并行，避免过度拆分）。每个 sub-agent 独立输出“发现的问题 + 证据”。
每个 sub-agent 必须引用“牢记评估标准”中对应条目，不得只做口头判断。
每个问题必须标注来源标准编号（例如：`[S3]` 表示“并发与生命周期”）。

### 责任矩阵（主责/次责）

- Sub-agent A（逻辑与架构）：主责 `S1`，次责 `S6`
- Sub-agent B（并发与生命周期）：主责 `S3`，次责 `S5`
- Sub-agent C（安全稳定与性能）：主责 `S2` + `S4`，次责 `S5`
- Sub-agent D（复用、兼容、文档测试）：主责 `S6`，次责 `S1` + `S5`

规则：

- 主责标准必须全量覆盖；次责标准只需覆盖与本次改动直接相关的部分。
- 若某问题跨多个标准，允许多标记（如 `[S2][S4]`）。
- 不允许多个 agent 报告同一问题的重复结论；若重复，保留证据更完整的一条。

### Sub-agent A: 逻辑正确性与架构一致性

- 业务逻辑是否完整，是否存在边界漏处理、状态不一致、错误传播断裂。
- 与 LoongCollector 架构约束是否一致（输入/处理/输出职责、Runner 模式、配置注册模式）。
- 是否引入隐式依赖、循环依赖或故障传播不可观测的问题。
- 重点覆盖评估标准：业务与架构、可维护性与兼容性。

### Sub-agent B: 并发、异步与生命周期

- 锁粒度、锁顺序、数据竞争、线程退出路径是否安全。
- 回调/异步流程是否存在竞态、悬空引用、未处理失败路径。
- 新增线程/定时任务是否可控停止，是否符合项目既有模式。
- 重点覆盖评估标准：并发与生命周期、稳定性与可观测。
- 生命周期/资源管理必查细则（必须逐项核对，重点是“正确释放与状态恢复”）：
  - 资源释放闭环：
    - 每条路径（启动失败、热更新替换、删除配置、进程退出）都要核对资源闭环：
      - 线程/future 可退出并被回收
      - queue pop 被 disable 后不再悬挂
      - 插件/Go pipeline 可停止且不残留引用
      - flush/batch/checkpoint 落盘语义与路径一致
  - 死锁与卡死风险：
    - 锁顺序是否跨模块一致（pipeline manager / queue manager / file server）。
    - `WaitAllItemsInProcessFinished`、队列 `Wait/Trigger`、`HoldOn/Resume` 是否可能形成循环等待。
    - 长等待仅告警不终止的路径，是否可能导致永久卡住或退出超时。
  - 状态恢复正确性（核心）：
    - 热加载后是否恢复到“可继续采集+处理+发送”的一致状态，而非部分组件已恢复。
    - 文件采集 `Pause -> Dump -> ClearCache -> Resume` 后，handler/checkpoint/缓存三者是否一致。
    - 配置失败回滚时，旧 pipeline/task 是否保持可用，不出现半更新状态。
  - 顺序检查作为辅证（不是唯一判据）：
    - 仍需核对关键顺序（runner init 顺序、pipeline start/stop 顺序），但结论必须落到资源与状态结果。

### Sub-agent C: 安全、稳定性与性能

- 输入校验、异常处理、重试退避、资源释放（RAII）是否完备。
- 右值/所有权：核验【调用点-传参-消费点】全链路，防止异常转移或冗余拷贝。
- 是否存在热路径性能回退（重复计算、拷贝、容器增长失控、高频日志刷屏）。
- 监控指标/告警是否完整，是否满足自监控规范。
- 重点覆盖评估标准：正确性与安全、性能与资源、稳定性与可观测。
- Checkpoint 必查细则（按改动范围选择）：
  - onetime checkpoint：
    - 启动时 `LoadCheckpointFile()`，配置变化后 `DumpCheckpointFile()`。
    - 超时删除、`RemoveConfig()` 与 checkpoint 文件是否保持一致，避免残留条目导致错误恢复。
  - file checkpoint（v1）：
    - `FileServer::Start()` 是否仍保持 `LoadCheckPoint()` 在前、注册 handler 在后。
    - `Pause/Stop` 是否保证 `DumpCheckPointToLocal()`，以及失败场景是否有可定位日志/告警。
  - exactly-once checkpoint（v2）：
    - 主 checkpoint 与 range checkpoint 是否成对维护，避免孤儿 key。
    - 扫描与 GC 逻辑是否可能误删活跃 checkpoint，或导致恢复时状态不连续。

### Sub-agent D: 复用合规与文档一致性

- 是否重复实现了已有公共能力（优先复用 `core/common` 与现有工具函数）。
- 注释与代码行为是否一致，TODO/FIXME 是否引入新技术债。
- 插件配置或 `GetXxxParam` 改动是否同步更新 `docs/` 对应文档。
- 重点覆盖评估标准：可维护性、兼容性与文档测试。

## Phase 5: Final Report（最终输出）

Final Report 偏实用交付，可直接用于落地修复和平台流转。它与 Phase 2 的“理解文档”并行存在、互不替代。

### Phase 5 输出要求（实用导向）

1. 先给 **Findings**，按严重度排序：`Critical` > `High` > `Medium` > `Low`。
2. 每个问题必须包含可定位证据与可执行建议。
3. 若未发现问题，明确写出“未发现阻断问题”，并列出残余风险与测试缺口。
4. 最后补充 **Highlights**（正向实践），简洁即可。
5. 必须包含 **Lifecycle Verdict**：
   - 资源释放：`PASS/FAIL`
   - 死锁/卡死风险：`PASS/FAIL`
   - 状态恢复正确性：`PASS/FAIL`
   - 每项附 1-3 条证据。
6. 必须包含 **Fix Plan**（按优先级分组）：
   - 立即修复（阻断合入）
   - 合入前修复
   - 可后续改进
7. 必须包含 **Validation Plan**（修复后怎么验证）：
   - 需要跑哪些测试、观察哪些指标、验证哪些告警与恢复路径。

### Final Report 落盘要求（必须写入 code-review 目录）

必须将 Final Report 写入仓库 `code-review/` 目录，禁止只在聊天中输出。

建议路径（与 Phase 2 同目录）：

- PR 评审：`code-review/pr-<number>/final-report.md`
- 分支评审：`code-review/branch-<branchName>/final-report.md`（`/` 替换为 `-`）

要求：

- `final-report.md` 必须引用对应的 `intent-architecture-notes.md`（相对路径链接）。
- 若执行了平台发布（PR评论/Review），在文档末尾记录发布链接；若失败，记录失败原因与重试命令。

问题输出格式：

```markdown
- Severity: <Critical|High|Medium|Low>
  - File: [<路径>:<起始行号>](file://./<路径>#L<起始行号>)
  - 问题: <一句话说明问题本质>
  - 影响: <可能导致的错误行为/风险>
  - 建议: <可直接执行的修复建议，必要时给最小代码片段>
```

额外要求：

- 行号必须在最终输出前重新核对，确保可点击跳转。
- 仅评论真实变更范围内的问题，避免“顺手重构建议”淹没核心缺陷。
- 语气专业、直接、简洁，优先给出可验证结论。

### 平台发布（可选但推荐）

若当前评审场景是 PR/分支评审，且工具可用，请在用户要求发布后自动化发布 Final Report：

- 必须等待用户显式确认后才能执行发布。
- 发布结构：
  1) **Inline Findings**：将可定位的问题逐条作为代码行内评论发布（不是回复到 PR 主评论）。
  2) **PR 摘要评论**：将Final Report 摘要回复到 PR 主评论。
     - 必含：Critical/High/Medium/Low 数量统计表、Lifecycle PASS/FAIL 表格、Lifecycle FAIL 证据、总体结论、Highlights。
     - 不含：不重复贴全部 findings。
- 发布工具：
  - 优先使用 `gh` 工具提交结构化评审结果；若环境存在 GitHub MCP，可等价使用 MCP。
  - Inline 评论建议使用 `gh api repos/<owner>/<repo>/pulls/<pr>/comments`（需包含 `commit_id/path/line/side/body`）。
  - 摘要评论建议使用 `gh pr comment <pr> --body-file <summary.md>`。
- 若发布失败，必须在输出中说明失败原因并给出可复制的发布内容。
