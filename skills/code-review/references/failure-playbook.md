# Code-Review Failure Playbook

本文件是故障恢复决策表，目标是让 agent 在异常时做正确分流：自动恢复、回退流程、或请求人工介入。

## 总原则

- 优先判断当前是首次还是非首次。
- 不手工拼接 JSON；恢复后必须回到标准流程节点继续执行。
- Preflight 相关异常默认人工介入，其余优先自动回退到可重建节点。
- 若脚本失败但不影响代码读取，允许降级继续评审，同时必须输出失败反馈记录。

## 场景 1：Preflight 失败（人工介入）

- 触发信号：`python3 --version` / `git rev-parse --is-inside-work-tree` / `gh auth status` 任一失败
- 决策：停止自动执行，提示用户介入检查环境与认证
- 动作级别：`manual_required`
- 返回节点：Preflight（三条检查全部通过后再进入 Phase 1）

## 场景 2：首次运行缺文件（正常入口，不是失败）

- 触发信号：`code-review/<target>/` 不存在，或缺少 `meta.json` / `reviewed_commits.json` / `comments/*`
- 决策：判定为 Bootstrap，走初始化流程
- 动作级别：`auto_recover`
- 返回节点：Phase 1-步骤 1（初始化）并顺序继续

## 场景 3：非首次运行时输入 schema 非法

- 触发信号：`invalid review-comments.json` / `invalid comment-status.json`
- 决策：放弃损坏中间态，回退到 Bootstrap 重建关键输入
- 动作级别：`auto_recover`
- 返回节点：Phase 1-步骤 1（初始化）-> 步骤 2（拉取 comments）-> 步骤 3（重建状态）

## 场景 4：commit 对象缺失 / commit 范围构建失败

- 触发信号：`missing base/head commit object` 或 `failed to build commit range`
- 决策：先自动同步 git 对象；若仍失败，转人工确认 base/head 选择
- 动作级别：`auto_then_manual`
- 返回节点：
  - 自动恢复成功：Phase 1-步骤 5（增量映射）
  - 自动恢复失败：人工确认后重跑步骤 5

## 场景 5：snapshot 目录平铺

- 触发信号：`snapshot/` 下没有源码相对路径层级（仅平铺文件）
- 决策：视为快照过程异常，清空并重建快照
- 动作级别：`auto_recover`
- 返回节点：快照生成步骤（完成后继续技术状态复核）

## 场景 6：脚本运行异常但可继续评审

- 触发信号：任意脚本报错，但仓库代码与基础 git/gh 能力仍可读取
- 决策：允许降级继续评审，避免流程阻塞；并强制记录失败反馈用于迭代 skill
- 动作级别：`degrade_continue`
- 必做动作：
  - 写入 `code-review/<target>/script-failures.md`（脚本名、命令、错误、时间、补偿动作）
  - 评审策略一律切换到 `full` 全量评审
  - 在 `final-report.md` 增加 “Script Failure Feedback” 小节
- 返回节点：当前评审阶段（按降级策略继续）

## 动作级别定义

- `manual_required`：必须人工介入后才能继续
- `auto_recover`：agent 可自动恢复并继续流程
- `auto_then_manual`：先自动尝试，失败后升级人工
- `degrade_continue`：允许继续评审，但必须记录失败并反馈
