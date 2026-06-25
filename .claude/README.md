# Claude Code

## 通用 AI 资产

本仓库的 skills 与 rules 统一维护在 [`.skill/`](../.skill/README.md)，与 Agent 工具无关。Claude Code 加载项目知识时请优先引用 `.skill/skills/` 与 `.skill/rules/`。

## OMC（Claude Code 专用）

[oh-my-claudecode (OMC)](https://github.com/anthropics/claude-code) 是多 Agent 编排插件，**仅适用于 Claude Code**，不属于通用 `.skill` 资产。

| 文件 | 用途 |
|------|------|
| `CLAUDE.md` | OMC 会话启动指令（插件自动维护） |
| `settings.json` | 启用 `oh-my-claudecode@omc` 插件 |
| `skills/omc-reference/SKILL.md` | OMC 代理目录、工具与技能注册表 |

通用 skill（compile、e2e、code-review 等）请从 `.skill/` 加载，不要在本目录重复维护。
