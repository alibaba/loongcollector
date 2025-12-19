# input_journal

## 简介

`input_journal` 插件用于收集 systemd journal 日志。支持灵活的过滤配置，可以按服务单元、标识符、内核日志和自定义模式进行日志收集。

## 配置参数

| 参数 | 类型 | 是否必选 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| Type | String | 是 | - | 插件类型，固定为 `input_journal` |
| JournalPaths | String数组 | 否 | `[]` | journal 文件路径，为空则使用系统默认路径 |
| SeekPosition | String | 否 | `tail` | 初始读取位置，可选值：`head`（从头开始）、`tail`（从尾部开始）、`cursor`（从上次保存的位置开始） |
| CursorSeekFallback | String | 否 | `head` | 当游标失效时的回退位置，可选值：`head` 或 `tail` |
| ResetIntervalSecond | Int | 否 | `3600` | 检查点重置间隔（秒） |
| Units | String数组 | 否 | `[]` | 要监控的 systemd 服务单元列表 |
| Kernel | Boolean | 否 | `false` | 是否收集内核日志（需同时配置 Units） |
| Identifiers | String数组 | 否 | `[]` | 要监控的 syslog 标识符列表 |
| MatchPatterns | String数组 | 否 | `[]` | 自定义匹配模式，支持 journal 字段匹配 |
| ParseSyslogFacility | Boolean | 否 | `false` | 是否将 facility 编号转换为名称 |
| ParsePriority | Boolean | 否 | `false` | 是否将 priority 编号转换为名称 |
| UseJournalEventTime | Boolean | 否 | `false` | 是否使用 journal 事件时间（否则使用采集时间） |

## 过滤逻辑

所有过滤器使用**或（OR）逻辑**，即满足任一过滤条件的日志都会被收集：

```text
(Units 中的任一服务) OR (Identifiers 中的任一标识符) OR (内核日志) OR (MatchPatterns 中的任一模式)
```

### 过滤器类型说明

#### 1. Units（服务单元过滤）

指定要收集的 systemd 服务单元日志，例如：`nginx.service`、`mysql.service`

#### 2. Identifiers（标识符过滤）

指定要收集的 syslog 标识符日志，例如：`sshd`、`systemd`

#### 3. Kernel（内核日志过滤）

收集内核日志（等同于 dmesg）。

**注意**：内核过滤需要同时满足两个条件：

- `Units` 不为空
- `Kernel` 设置为 `true`

如需单独收集内核日志，可使用 `MatchPatterns: ["_TRANSPORT=kernel"]`

#### 4. MatchPatterns（自定义模式过滤）

支持自定义 journal 字段匹配，例如：

- `_SYSTEMD_USER_UNIT=myapp.service` - 匹配用户服务
- `PRIORITY=3` - 匹配特定优先级
- `_COMM=nginx` - 匹配特定命令
- `_UID=0` - 匹配特定用户ID

## Checkpoint 机制

### 功能说明

插件支持断点续传功能，通过 checkpoint 机制记录已读取的日志位置，确保在重启或故障恢复后能够从上次停止的位置继续读取，避免日志丢失或重复采集。

### 工作原理

1. **位置记录**：插件会定期保存当前读取到的 journal cursor（游标位置）
2. **恢复读取**：重启后通过 `SeekPosition: "cursor"` 从上次保存的位置继续读取
3. **游标失效**：如果保存的游标失效（如日志文件被删除或轮转），则根据 `CursorSeekFallback` 配置回退到 `head` 或 `tail`
4. **定期重置**：通过 `ResetIntervalSecond` 参数控制检查点重置间隔，避免长时间累积导致的性能问题

### 相关配置

```json
{
    "Type": "input_journal",
    "SeekPosition": "cursor",        // 使用 cursor 模式启用断点续传
    "CursorSeekFallback": "tail",    // 游标失效时从尾部开始
    "ResetIntervalSecond": 3600      // 每小时重置一次检查点
}
```

## 配置示例

### 示例1：监控 Web 服务

```json
{
    "Type": "input_journal",
    "Units": ["nginx.service", "mysql.service"],
    "Kernel": true,
    "SeekPosition": "tail"
}
```

### 示例2：系统级监控

```json
{
    "Type": "input_journal",
    "Units": ["systemd-networkd.service"],
    "Identifiers": ["sshd", "systemd"],
    "Kernel": true,
    "SeekPosition": "cursor",
    "ParsePriority": true
}
```

### 示例3：仅收集内核日志

```json
{
    "Type": "input_journal",
    "MatchPatterns": ["_TRANSPORT=kernel"]
}
```

### 示例4：高优先级告警

```json
{
    "Type": "input_journal",
    "MatchPatterns": ["PRIORITY=0", "PRIORITY=1", "PRIORITY=2"],
    "ParsePriority": true,
    "UseJournalEventTime": true
}
```

## 常见问题

### 1. 设置了 `Kernel: true` 但未收集到内核日志

**原因**：内核过滤需要同时配置 `Units`

**解决方案**：

```json
{
    "Units": ["any.service"],  // 添加至少一个服务单元
    "Kernel": true
}
```

或使用 MatchPatterns：

```json
{
    "MatchPatterns": ["_TRANSPORT=kernel"]
}
```

### 2. 日志采集位置不正确

**解决方案**：

- 从头开始采集：设置 `"SeekPosition": "head"`
- 仅采集新日志：设置 `"SeekPosition": "tail"`
- 断点续传：设置 `"SeekPosition": "cursor"`

### 3. 如何验证配置是否正确

使用 journalctl 命令手动测试：

```bash
# 测试服务单元过滤
journalctl -u nginx.service

# 测试标识符过滤
journalctl -t sshd

# 查看可用字段
journalctl --list-fields
```
