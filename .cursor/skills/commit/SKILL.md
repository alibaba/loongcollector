---
name: commit
description: Writing Commit Message
---
生成符合 Conventional Commits 规范的提交信息。  

type: feat | fix | docs | style | refactor | perf | test | chore | revert
scope: 可选，文件/模块/子系统，如 api, ui, auth, deps
subject: ≤ 50 字符，用祈使句，首字母小写，不加句号
body: 每行 ≤ 72 字符，解释「做了什么」与「为什么做」
footer: 可选，关联 Issue / PR / Breaking Change 等

## Commit Message 快速模板

{TYPE}({SCOPE}): {动词+对象}

{为什么需要这次改动，带来哪些用户/系统影响}

Fixes #{ISSUE_ID}

## 具体步骤

1. 信息收集

- 读取 git diff。如果用户已经提供，则不需要执行。

2. 内容提炼

- 判断变更属于哪一类 type。
- 若改动跨多个 scope，使用最核心模块作为 scope。
- 提取「新增 / 修改 / 删除」的函数、类、接口名，写入 subject。
- 若存在破坏性变更，在 footer 加 BREAKING CHANGE: 说明。

3. 结果输出

- 先给出完整 commit 供用户确认，再询问：是否直接使用 git commit -m "<message>"？（y/N）

## 禁止行为

- 不得使用 update code, fix bug, wip 等无意义描述。
- 禁止出现超过 72 字符的 subject 或 body 行。
- 禁止把 issue 链接放在 subject 行。
