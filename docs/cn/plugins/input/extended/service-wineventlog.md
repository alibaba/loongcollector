# Windows 事件日志

## 简介

`service_wineventlog` 插件用于持续采集 Windows 事件日志。默认采集 `Application` 通道，并在 LoongCollector 重启后通过 checkpoint 从上次成功处理的位置继续采集。

在 Windows Vista 及以上系统中，插件优先使用 Windows Event Log API；该 API 支持按事件 ID、级别和 Provider 过滤。旧版 Windows 或新 API 不可用时，插件会回退到传统 Event Logging API。

源代码：[input_wineventlog](https://github.com/alibaba/loongcollector/tree/main/plugins/input/input_wineventlog)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| ✓ | — | — |

## 前置条件

- 运行环境为 Windows。
- 配置的事件通道必须存在，且运行 LoongCollector 的账户需要具备读取该通道的权限。
- 使用 `EventID`、`Level` 或 `Provider` 过滤时，需要 Windows Vista 或更高版本的 Windows Event Log API 支持。

## 配置参数

| 参数 | 类型 | 是否必选 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| Type | string | 是 | / | 固定为 `service_wineventlog`。 |
| Name | string | 否 | `Application` | 要采集的 Windows 事件日志通道名称，例如 `Application`、`System`、`Security` 或 `Microsoft-Windows-PowerShell/Operational`。 |
| IgnoreOlder | uint | 否 | `0` | 忽略早于该秒数的事件。`0` 表示不按事件时间过滤。 |
| EventID | string | 否 | 空 | 事件 ID 白名单和黑名单，多个条件以逗号分隔。支持单个 ID（如 `4624`）、范围（如 `4700-4800`）和排除项（如 `-4735`）。仅 Windows Event Log API 可用时生效。 |
| Level | string | 否 | `information,warning,error,critical` | 要采集的级别，多个级别以逗号分隔。可用值为 `verbose`/`5`、`information`/`info`/`4`、`warning`/`warn`/`3`、`error`/`err`/`2`、`critical`/`crit`/`1` 和 `0`。仅 Windows Event Log API 可用时生效。 |
| Provider | string[] | 否 | 空 | 要采集的 Provider（来源）名称列表。为空时不按 Provider 过滤。仅 Windows Event Log API 可用时生效。 |
| IgnoreZeroValue | bool | 否 | `false` | 是否省略值为空字符串或数值为 `0` 的输出字段。 |
| WaitInterval | uint | 否 | `1` | 当前没有可读事件时再次读取前的等待秒数。 |

> Windows Event Log API 不可用时，插件会使用传统 Event Logging API。此时 `IgnoreOlder` 仍生效，但 `EventID`、`Level` 和 `Provider` 过滤不可用。

## 输出字段

每条事件输出为日志字段。字段值均以字符串形式写出；`IgnoreZeroValue=false` 时，空值字段也会保留。

| 字段 | 说明 |
| --- | --- |
| `type` | 读取事件所用的 API：`wineventlog` 或 `eventlogging`。 |
| `log_name` | Windows 事件日志通道名称。 |
| `source_name` | 事件 Provider（来源）名称。 |
| `computer_name` | 产生事件的计算机名称。 |
| `record_number` | 事件记录号。 |
| `event_id` | Windows 事件 ID。 |
| `xml` | 事件的 XML 表示；仅在 Windows Event Log API 能够生成 XML 时有内容。 |
| `provider_guid` | 事件 Provider GUID。 |
| `version` | 事件版本。 |
| `level` | 事件级别。 |
| `task` | 事件任务类别。 |
| `opcode` | 事件操作码。 |
| `keywords` | 事件关键字。 |
| `message` | Windows 渲染后的事件消息。 |
| `message_error` | 事件消息渲染过程中的错误信息。 |
| `activity_id` | 关联活动 ID。 |
| `related_activity_id` | 关联的父活动 ID。 |
| `process_id` | 产生事件的进程 ID。 |
| `thread_id` | 产生事件的线程 ID。 |
| `processor_id` | 产生事件的处理器 ID。 |
| `session_id` | Windows 会话 ID。 |
| `kernel_time` | 内核执行时间。 |
| `user_time` | 用户态执行时间。 |
| `processor_time` | 处理器执行时间。 |
| `user_identifier` | 用户 SID。 |
| `user_name` | 解析后的用户名。 |
| `user_domain` | 解析后的用户域。 |
| `user_type` | 用户账户类型。 |
| `event_data` | 事件数据字段的 JSON 对象。无名称的 `Data` 字段按 `Data1`、`Data2` 等键保存。 |
| `user_data` | 用户数据字段的 JSON 对象；包含 `xml_name`。 |

## 样例

### 采集 Application 和 System 中的错误与关键事件

```yaml
enable: true
inputs:
  - Type: service_wineventlog
    Name: Application
    Level: error,critical
    WaitInterval: 1
  - Type: service_wineventlog
    Name: System
    Level: error,critical
    WaitInterval: 1
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

### 采集 Windows 安全登录事件

下面的配置只采集 `Security` 通道中的成功登录（4624）和登录失败（4625）事件。读取 Security 通道通常需要以具备相应权限的账户运行。

```yaml
enable: true
inputs:
  - Type: service_wineventlog
    Name: Security
    EventID: 4624,4625
    Provider:
      - Microsoft-Windows-Security-Auditing
    IgnoreZeroValue: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

### 采集 PowerShell 操作日志

```yaml
enable: true
inputs:
  - Type: service_wineventlog
    Name: Microsoft-Windows-PowerShell/Operational
    EventID: 4103,4104
    Level: verbose,information,warning,error,critical
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```