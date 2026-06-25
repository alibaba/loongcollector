# LoongCollector AI 资产

本目录是 LoongCollector 仓库 AI 辅助编码资产的**唯一来源**，与具体 Agent 工具无关。任何 AI 编程助手（IDE Agent、CLI Agent、Cloud Agent 等）均应从此处加载 skills 与 rules。

## 目录结构

```text
.skill/
├── skills/          # 可复用任务流程（评审、构建、E2E、安全检查等）
├── rules/           # 项目级约束与知识（编码规范、架构、测试标准等）
└── examples/        # 可选配置示例
```

## skills 与 rules 的区别

- **skills**：指导 Agent 如何执行具体任务（步骤、脚本、模板）。
- **rules**：约束 Agent 行为与项目知识（编码规范、架构说明）。

## 维护说明

- 新增或修改 skill / rule 时，**只编辑本目录**。
- 文件内路径引用统一使用 `.skill/skills/` 或 `.skill/rules/` 前缀。
- 不要在 skill / rule 正文中写入特定 Agent 产品名称。

## 顶层 Skills

| Skill | 用途 |
|-------|------|
| `code-review` | 代码评审流程与方法 |
| `commit` | 提交信息编写 |
| `compile` | 构建与单元测试 |
| `design-document` | 设计文档编写规范 |
| `e2e` | E2E 测试全流程 |
| `e2e-develop-guide` | E2E 框架开发指南 |
| `e2e-manual` | E2E 手工测试流程 |
| `mermaid` | Mermaid 图表规范 |
| `project-knowledge` | 项目架构、术语、代码库地图 |
| `review-standards` | 代码评审行为标准 |
| `riper5-protocol` | RIPER-5 复杂任务工作协议 |
| `security-check` | 提交/推送前安全检查 |
| `selfmonitor` | 自监控与告警规范 |
| `testing-standards` | 测试标准总览 |

Rules 详见 `rules/README`。
