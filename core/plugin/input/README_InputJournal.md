# InputJournal Plugin

## 概述

InputJournal 是一个 C++ 实现的 systemd journal 日志收集插件，用于从 systemd journal 中读取日志条目并发送到日志处理管道。

## 功能特性

- **实时日志收集**: 支持实时跟随 journal 日志
- **灵活的过滤**: 支持按 unit、identifier、pattern 等条件过滤
- **检查点支持**: 支持重启后从上次位置继续收集
- **内存管理**: 定期重启 journal 连接以释放内存
- **错误恢复**: 网络或 journal 错误时的自动重试机制
- **时间戳处理**: 支持使用 journal 原始时间戳或当前时间

## 配置参数

### 基本参数

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `SeekPosition` | string | "tail" | 起始位置：cursor/head/tail |
| `CursorFlushPeriodMs` | int | 5000 | 检查点保存间隔（毫秒） |
| `CursorSeekFallback` | string | "tail" | 检查点失败时的回退位置 |
| `ResetIntervalSecond` | int | 3600 | journal 重启间隔（秒） |

### 过滤参数

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `Units` | array | [] | 监控的 systemd 单元列表 |
| `Kernel` | bool | true | 是否包含内核日志 |
| `Identifiers` | array | [] | syslog 标识符列表 |
| `MatchPatterns` | array | [] | 自定义匹配模式列表 |

### 解析参数

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `ParseSyslogFacility` | bool | false | 是否解析 syslog facility |
| `ParsePriority` | bool | false | 是否解析优先级 |
| `UseJournalEventTime` | bool | false | 是否使用 journal 时间戳 |

## 配置示例

```json
{
  "type": "input_journal",
  "detail": {
    "SeekPosition": "tail",
    "Units": ["nginx.service", "docker.service"],
    "Kernel": true,
    "Identifiers": ["nginx", "docker"],
    "ParseSyslogFacility": true,
    "ParsePriority": true,
    "UseJournalEventTime": true,
    "ResetIntervalSecond": 3600
  }
}
```

## 工作原理

1. **初始化**: 连接到 systemd journal，设置过滤条件
2. **定位**: 根据配置定位到合适的日志位置
3. **收集**: 持续读取新的日志条目
4. **处理**: 解析字段，转换格式，发送到输出
5. **检查点**: 定期保存读取位置，支持重启恢复

## 使用场景

- 系统日志监控和分析
- 容器环境中的日志收集
- 微服务架构的日志聚合
- 系统故障排查和审计

## 注意事项

- 目前使用模拟实现，实际生产环境需要集成真实的 systemd-journal 库
- 检查点功能需要进一步实现
- 日志输出到管道的功能需要完善
- 建议在 Linux 系统上使用

## 开发状态

- [x] 基本框架实现
- [x] 配置解析
- [x] journal 读取逻辑
- [x] 过滤和匹配功能
- [ ] 真实的 systemd-journal 集成
- [ ] 检查点功能
- [ ] 日志输出到管道
- [ ] 单元测试
- [ ] 性能优化

## 依赖

- C++17 或更高版本
- systemd-journal 开发库（生产环境）
- iLogtail 核心框架

## 贡献

欢迎提交 Issue 和 Pull Request 来改进这个插件。 