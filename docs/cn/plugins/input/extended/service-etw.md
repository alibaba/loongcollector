# service_etw

## 描述

`service_etw` 插件是一个通用的 Windows ETW（Event Tracing for Windows）实时事件采集插件。

ETW 是 Windows 内核级别的高性能事件追踪机制，广泛用于系统诊断、安全审计和性能分析。本插件支持订阅任意已注册的 ETW Provider，实时采集事件并输出到下游。

### 典型使用场景

- DNS Server 查询日志采集（DNS Analytical Channel）
- PowerShell 脚本执行审计
- 进程创建/退出监控
- 网络连接追踪
- 自定义应用的 ETW 事件采集

## 前置条件

- Windows Server 2012 及以上版本
- 以**管理员权限**运行 Loongcollector（ETW Session 需要 `SeSystemProfilePrivilege`）
- 目标 ETW Provider 已在系统中注册

## 配置参数

| 参数 | 类型 | 是否必选 | 默认值 | 说明 |
|------|------|---------|--------|------|
| Type | string | 是 | / | 固定为 `service_etw` |
| ProviderName | string | 与 ProviderGUID 二选一 | 空 | ETW Provider 名称，运行时通过 Windows 注册表解析为 GUID。例如 `Microsoft-Windows-DNSServer` |
| ProviderGUID | string | 与 ProviderName 二选一 | 空 | ETW Provider GUID，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}` |
| Level | int | 否 | 4 | ETW Trace Level。1=Critical, 2=Error, 3=Warning, 4=Informational, 5=Verbose |
| Keywords | uint64 | 否 | 0 | ETW Keywords 位掩码，用于过滤特定事件类别。0 表示采集所有事件 |

> **ProviderName vs ProviderGUID**：推荐使用 `ProviderName`，可读性更好且无需查找 GUID。插件启动时会自动通过 Windows 注册表将名称解析为 GUID。如果同时指定了两者，`ProviderName` 优先。

## 采集字段

每条 ETW 事件会输出以下字段：

### 通用 Header 字段

| 字段 | 类型 | 说明 |
|------|------|------|
| event_id | string | ETW 事件 ID |
| opcode | string | 操作码 |
| level | string | 事件级别（1-5） |
| keywords | string | Keywords 位掩码（十六进制） |
| process_id | string | 产生事件的进程 ID |
| thread_id | string | 产生事件的线程 ID |

### 动态属性字段

除上述 Header 字段外，插件会自动提取 ETW 事件的所有自定义属性（EventProperties），以 `key=value` 形式输出。具体字段取决于 ETW Provider 的事件定义。

**DNS Server 示例字段**：

| 字段 | 说明 |
|------|------|
| QNAME | 查询域名（FQDN） |
| QTYPE | 查询类型（1=A, 28=AAAA, 5=CNAME 等） |
| Source | 客户端 IP |
| Destination | 目标 DNS IP |
| XID | DNS 事务 ID |
| TCP | 0=UDP, 1=TCP |
| PacketData | 原始 DNS 报文（十六进制） |

## 配置示例

### 示例 1：采集 DNS Server Analytical 日志

```yaml
enable: true
inputs:
  - Type: service_etw
    ProviderName: "Microsoft-Windows-DNSServer"
    Level: 4
    Keywords: 0x8000000000000037
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

Keywords 位掩码说明：
- `0x8000000000000000` — Analytical Channel（必须）
- `0x0000000000000001` — QUERY_RECEIVED
- `0x0000000000000002` — RESPONSE_SUCCESS
- `0x0000000000000004` — RESPONSE_FAILURE
- `0x0000000000000010` — RECURSE_QUERY_OUT
- `0x0000000000000020` — RECURSE_RESPONSE_IN

### 示例 2：采集 PowerShell 脚本执行事件

```yaml
enable: true
inputs:
  - Type: service_etw
    ProviderName: "Microsoft-Windows-PowerShell"
    Level: 5
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

### 示例 3：使用 GUID 直接指定 Provider

```yaml
enable: true
inputs:
  - Type: service_etw
    ProviderGUID: "{EB79061A-A566-4698-9119-3ED2807060E7}"
    Level: 4
    Keywords: 0x8000000000000037
flushers:
  - Type: flusher_sls
```

## 使用限制

| 限制项 | 说明 |
|--------|------|
| 操作系统 | 仅支持 Windows Server 2012 R2 及以上版本 |
| 权限要求 | 必须以管理员权限运行 loongcollector |
| Provider 注册 | 目标 ETW Provider 必须已在系统中注册 |
| 并发 Session | 同一 Provider 不建议同时开启多个 ETW Session |
| Analytical Channel | 采集 Analytical/Debug Channel 需在 Keywords 中设置对应位 |

## 常见问题

### ETW Session 创建失败

- 确认以管理员权限运行
- 检查是否有同名 ETW Session 残留：`logman query -ets`
- 确认目标 Provider 已注册：`logman query providers | findstr <ProviderName>`

### ProviderName 解析失败

- 确认 Provider 名称拼写正确（大小写不敏感）
- 在注册表中验证：`reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\WINEVT\Publishers" /s /f "<ProviderName>"`

### 无事件输出

- 确认 Level 和 Keywords 配置正确
- 某些 Provider 需要特定 Keywords 才会产生事件
- 使用 `flusher_stdout` 先验证数据流是否正常
