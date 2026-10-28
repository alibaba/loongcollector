# LoongCollector — Agent 指南

本文件是跨 AI 编程助手的**项目级上下文**（ambient context）。可复用任务流程见 [`skills/`](skills/)，Cursor 专用规则见 [`.cursor/rules/`](.cursor/rules/README)。

## 项目概览

LoongCollector 是高性能可观测性数据采集 Agent（C++ 核心 + Go 插件层）。数据流：**Input → Processor → Aggregator → Flusher**。

| 目录 | 说明 |
|------|------|
| `core/` | C++ 核心：pipeline、文件采集、eBPF、Prometheus、序列化等 |
| `plugins/`、`pluginmanager/`、`plugin_main/` | Go 插件与插件管理 |
| `e2e/`、`test/` | 集成与 E2E 测试 |
| `docs/` | 项目文档 |
| `scripts/` | 构建、测试脚本 |

## 常用命令

```bash
make plugin_local          # 构建 Go 插件库
make unittest_plugin       # Go 插件单元测试
make unittest_pluginmanager
make e2e                   # E2E 测试
make lint                  # golangci-lint
make check-license
make clean
```

C++ 核心单测在 `core/unittest/`；与 CI 对齐的编译见 `skills/compile/SKILL.md`。

## AI 资产布局

```text
AGENTS.md               # 项目上下文 SSOT
CLAUDE.md → AGENTS.md   # Claude Code 读取
skills/                 # SSOT：可复用任务流程（步骤、脚本、模板）
.cursor/rules/          # Cursor 专用规则（编码规范、架构约束）
.cursor/skills → skills # symlink
.cursor/examples/mcp.json.example  # MCP 配置示例（可选）
.claude/skills → skills # symlink（含 settings.json 启用 OMC 等 Claude 专用配置）
```

- 新增或修改 **skill**：只编辑 `skills/<name>/SKILL.md`；路径引用用 `skills/` 前缀。
- 新增或修改 **rule**：只编辑 `.cursor/rules/`；引用规则用 `.cursor/rules/` 前缀。
- 不要在 `.cursor/skills/` 或 `.claude/skills/` 下维护副本。

## Skills

| Skill | 用途 |
|-------|------|
| `project-knowledge` | 架构、术语、代码库地图 |
| `code-review` | 代码评审流程 |
| `review-standards` | 代码评审行为标准 |
| `compile` | 构建与单测 |
| `e2e` | E2E 测试全流程 |
| `e2e-develop-guide` | E2E 框架开发指南 |
| `e2e-manual` | E2E 手工测试流程 |
| `testing-standards` | 测试标准总览 |
| `commit` | 提交信息规范 |
| `security-check` | 提交/推送前安全检查 |
| `design-document` | 设计文档编写规范 |
| `mermaid` | Mermaid 图表规范 |
| `riper5-protocol` | RIPER-5 复杂任务工作协议 |
| `selfmonitor` | 自监控与告警规范 |
| `omc-reference` | OMC 代理目录（Claude Code 专用） |

新增或删除 skill 时，同步更新上表。

## 编码约定

- **Commit**：Conventional Commits — `{type}({scope}): {subject}`，详见 `skills/commit/SKILL.md`
- **C++**：PascalCase 类/公开方法，camelCase 变量/私有方法，`m` 前缀成员，`k` 前缀常量
- **Go**：标准 gofmt/go vet，错误用 `fmt.Errorf("context: %w", err)` 包装

详细规范见 `.cursor/rules/coding-standards/`。
