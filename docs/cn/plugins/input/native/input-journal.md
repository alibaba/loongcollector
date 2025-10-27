# InputJournal - 高性能Systemd日志收集器

![InputJournal](https://img.shields.io/badge/Component-JournalServer-blue) ![Platform](https://img.shields.io/badge/Platform-Linux-green) ![Language](https://img.shields.io/badge/Language-C%2B%2B-red)

InputJournal是一个高性能的systemd日志收集组件，专为高效可靠的日志数据提取而设计。它提供全面的过滤功能，并与Golang实现保持兼容性。

---

## ✨ 核心特性

- **🚀 高性能**: 为最大吞吐量优化的C++实现
- **🔧 灵活过滤**: 支持单元、标识符、内核日志和自定义模式过滤
- **⚡ 实时收集**: 实时日志流和检查点恢复
- **🛡️ 生产就绪**: 经过实战考验的全面错误处理
- **🎯 或逻辑**: 所有过滤器使用或逻辑，提供最大灵活性
- **📊 丰富元数据**: 收集全面的日志条目元数据


### 核心组件

| 组件 | 用途 | 描述 |
|-----------|---------|-------------|
| **JournalServer** | 主协调器 | 管理日志连接和数据流 |
| **连接管理器** | 连接处理 | 管理日志读取器连接和生命周期 |
| **日志过滤器** | 数据过滤 | 应用配置的或逻辑过滤器 |
| **日志读取器** | 数据提取 | 与systemd日志API接口 |

## 🎯 过滤系统

InputJournal提供一个复杂的过滤系统，允许精确控制收集哪些日志条目。**所有过滤器使用或逻辑**，意味着匹配任何配置过滤器的条目都将被收集。

### 过滤器类型

#### 1. 单元过滤器
收集来自特定systemd单元（服务、定时器等）的日志。

**配置:**
```json
{
  "units": ["nginx.service", "mysql.service", "redis.service"]
}
```

**匹配逻辑:**
- 服务消息: `_SYSTEMD_UNIT=nginx.service`
- 核心转储: `MESSAGE_ID=<coredump_id> + COREDUMP_UNIT=nginx.service`
- PID1消息: `_PID=1 + UNIT=nginx.service`
- 守护进程消息: `_UID=0 + OBJECT_SYSTEMD_UNIT=nginx.service`
- Slice消息: `_SYSTEMD_SLICE=nginx.service`

#### 2. 标识符过滤器
收集来自特定syslog标识符的日志。

**配置:**
```json
{
  "identifiers": ["sshd", "systemd", "kernel"]
}
```

**匹配逻辑:**
- `SYSLOG_IDENTIFIER=sshd`
- `SYSLOG_IDENTIFIER=systemd`
- `SYSLOG_IDENTIFIER=kernel`

#### 3. 内核过滤器 ⚠️
收集内核日志（相当于dmesg）。

**重要:** 内核过滤器仅在**同时满足两个条件**时激活:
- `units`已配置（不为空）
- `kernel`设置为`true`

**配置:**
```json
{
  "units": ["nginx.service"],
  "kernel": true
}
```

**匹配逻辑:**
- `_TRANSPORT=kernel`

**为什么有这个条件？**
这防止在没有定义特定收集目标时意外收集大量内核日志。

#### 4. 匹配模式过滤器
支持自定义日志字段匹配模式。

**配置:**
```json
{
  "matchPatterns": [
    "_SYSTEMD_USER_UNIT=myapp.service",
    "PRIORITY=3",
    "_COMM=nginx"
  ]
}
```

### 过滤器逻辑关系

所有过滤器使用**或逻辑**组合:

```
(单元1 或 单元2 或 单元3)
或
(标识符1 或 标识符2)  
或
(内核传输)
或
(模式1 或 模式2)
```

### 配置示例

#### 示例1: Web服务器 + 数据库监控
```json
{
  "units": ["nginx.service", "mysql.service"],
  "kernel": true,
  "identifiers": ["sshd"]
}
```
**结果:** 收集nginx、mysql、内核日志和SSH守护进程日志。

#### 示例2: 系统服务监控  
```json
{
  "units": ["systemd-networkd.service", "systemd-resolved.service"],
  "kernel": false,
  "matchPatterns": ["_UID=0"]
}
```
**结果:** 收集networkd、resolved服务和所有root用户进程。

#### 示例3: 仅内核收集
```json
{
  "matchPatterns": ["_TRANSPORT=kernel"]
}
```
**结果:** 仅收集内核日志（绕过units+kernel要求）。

#### 示例4: 高优先级警报
```json
{
  "matchPatterns": [
    "PRIORITY=0",
    "PRIORITY=1", 
    "PRIORITY=2"
  ]
}
```
**结果:** 仅收集紧急、警报和关键优先级消息。

## ⚙️ 配置参考

### 完整配置模式

```json
{
  "Type": "input_journal",
  "JournalPaths": ["/var/log/journal"],
  "SeekPosition": "tail",
  "CursorFlushPeriodMs": 1000,
  "CursorSeekFallback": "head",
  "ResetIntervalSecond": 3600,
  
  "Units": ["nginx.service", "mysql.service"],
  "Kernel": true,
  "Identifiers": ["sshd", "systemd"],
  "MatchPatterns": ["_UID=0"],
  
  "ParseSyslogFacility": true,
  "ParsePriority": true,
  "UseJournalEventTime": true
}
```

### 配置参数

| 参数 | 类型 | 默认值 | 描述 |
|-----------|------|---------|-------------|
| `JournalPaths` | 数组 | `[]` | 日志文件路径（空=系统日志） |
| `SeekPosition` | 字符串 | `"tail"` | 初始读取位置: `head`、`tail`、`cursor` |
| `CursorFlushPeriodMs` | 整数 | `1000` | 检查点保存频率（毫秒） |
| `CursorSeekFallback` | 字符串 | `"head"` | 游标无效时的回退位置（可选值：`head` 或 `tail`） |
| `ResetIntervalSecond` | 整数 | `3600` | 检查点重置间隔 |
| `Units` | 数组 | `[]` | 要监控的Systemd单元 |
| `Kernel` | 布尔值 | `false` | 启用内核日志收集 |
| `Identifiers` | 数组 | `[]` | 要监控的Syslog标识符 |
| `MatchPatterns` | 数组 | `[]` | 自定义匹配模式 |
| `ParseSyslogFacility` | 布尔值 | `false` | 将设施号转换为名称 |
| `ParsePriority` | 布尔值 | `false` | 将优先级号转换为名称 |
| `UseJournalEventTime` | 布尔值 | `false` | 使用日志时间戳而非当前时间 |

## 🚀 使用示例

### 基本服务监控
监控特定服务并收集内核日志:

```json
{
  "Type": "input_journal",
  "Units": ["nginx.service", "mysql.service"],
  "Kernel": true,
  "SeekPosition": "tail"
}
```

### 系统级监控
从多个源收集日志:

```json
{
  "Type": "input_journal",
  "Units": ["systemd-networkd.service"],
  "Identifiers": ["kernel", "systemd", "NetworkManager"],
  "Kernel": true,
  "MatchPatterns": ["_UID=0"],
  "ParsePriority": true
}
```

### 仅紧急警报
系统中的高优先级消息:

```json
{
  "Type": "service_journal_server",
  "MatchPatterns": [
    "PRIORITY=0",
    "PRIORITY=1",
    "PRIORITY=2"
  ],
  "ParsePriority": true,
  "UseJournalEventTime": true
}
```

## 🔧 构建和开发

### 前置要求
- 带有systemd的Linux
- CMake 3.16+
- GCC 9+ 或 Clang 10+
- libsystemd-dev

### 构建命令
```bash
# 从项目根目录
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make journal_server
```



## 🐛 故障排除

### 常见问题

#### 问题: 尽管设置了`kernel: true`但不收集内核日志
**原因:** 内核过滤器需要配置`units`。

**解决方案:**
```json
{
  "units": ["some.service"],  // 至少添加一个单元
  "kernel": true
}
```
或使用匹配模式:
```json
{
  "matchPatterns": ["_TRANSPORT=kernel"]
}
```

#### 问题: 多个过滤器时无日志收集
**原因:** 期望与逻辑而非或逻辑。

**解决方案:** 记住过滤器使用或逻辑。如果需要与逻辑，使用特定匹配模式:
```json
{
  "matchPatterns": ["_SYSTEMD_UNIT=nginx.service + PRIORITY=3"]
}
```

#### 问题: CPU使用率高
**原因:** 过滤器过于宽泛，收集了过多数据。

**解决方案:** 使用更具体的过滤器:
```json
{
  "units": ["specific.service"],
  "matchPatterns": ["PRIORITY=0", "PRIORITY=1", "PRIORITY=2"]
}
```

#### 问题: 缺少日志条目
**原因:** 
1. 错误的单元名称
2. 日志不可读
3. 检查点位置问题

**解决方案:**
1. 验证单元名称: `systemctl list-units`
2. 检查权限: `journalctl --verify`
3. 重置位置: `"SeekPosition": "head"`

### 调试提示

1. **启用调试日志** 在logtail配置中
2. **检查日志完整性**: `journalctl --verify`
3. **手动测试过滤器**: `journalctl -u nginx.service`
4. **监控检查点文件** 用于位置跟踪
5. **使用`journalctl --list-fields`** 发现可用字段


---

**📚 相关文档:**
- [LoongCollector主README](../../README.md)
- [插件开发指南](../../docs/en/guides/README.md)
- [性能基准](../../docs/en/concept&designs/README.md) 