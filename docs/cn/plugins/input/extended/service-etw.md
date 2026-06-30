# service_etw

## 简介

`service_etw` 是一个通用的 Windows ETW（Event Tracing for Windows）实时事件采集插件。

ETW 是 Windows 操作系统内置的高性能事件追踪框架，几乎所有 Windows 组件和服务都通过 ETW Provider 输出诊断和分析事件。本插件支持订阅任意 ETW Provider，实时采集事件并透传所有字段。

### 典型使用场景

- DNS Server 查询和响应事件采集（DNS Analytical Channel）
- PowerShell 脚本执行审计
- 安全事件/威胁检测
- 网络连接监控
- 其他自定义 ETW 事件采集


## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| ✓ | — | — |

## 前置条件

- Windows Server 2012 及以上版本
- 以**管理员权限**运行 Loongcollector（ETW Session 需要 `SeSystemProfilePrivilege`）
- 目标 ETW Provider 必须在系统中已注册

## 配置参数

| 参数 | 类型 | 是否必选 | 默认值 | 说明 |
|------|------|---------|--------|------|
| Type | string | 是 | / | 固定为 `service_etw` |
| ProviderName | string | 与 ProviderGUID 二选一 | 无 | ETW Provider 名称，插件会自动从 Windows 注册表解析 GUID。例如 `Microsoft-Windows-DNSServer` |
| ProviderGUID | string | 与 ProviderName 二选一 | 无 | ETW Provider GUID，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}` |
| Level | int | 否 | 4 | ETW Trace Level：1=Critical, 2=Error, 3=Warning, 4=Informational, 5=Verbose |
| Keywords | uint64 | 否 | 0 | ETW Keywords 位掩码，用于按类别过滤事件。0 表示不过滤 |
| DNSQueryDomainFilters | string[] | 否 | 空 | 仅对 `Microsoft-Windows-DNSServer` 生效，按 `dns_query` 丢弃 DNS 事件。支持精确域名和 `*.example.com` 形式的后缀通配；为空表示不丢弃 |
| ParsePacketData | bool | 否 | true | 仅对 DNS 生效，是否解析 `PacketData` 中的 DNS wire format 响应内容。关闭可降低 CPU 消耗 |
| AsyncProcess | bool | 否 | false | 是否将字段归一化、Provider 专属富化、过滤和写入 Collector 放到异步 worker 中处理，降低 ETW callback 阻塞风险；对所有 ETW Provider 生效 |
| EventQueueSize | int | 否 | 8192 | 仅 `AsyncProcess=true` 时生效，异步事件队列长度；最大 1000000 |
| WorkerCount | int | 否 | min(CPU, 2) | 仅 `AsyncProcess=true` 时生效，异步处理 worker 数量；最大 64。Collector 写入会串行化，避免依赖 Collector 并发安全 |
| DropWhenQueueFull | bool | 否 | false | `AsyncProcess=true` 时生效，队列满时是否丢弃新事件；false 表示阻塞 ETW callback |
| BufferSizeKB | uint32 | 否 | 0 | ETW 单个 buffer 大小，单位 KB；0 表示使用 Windows 默认值；配置时范围为 4..4096 |
| MinBuffers | uint32 | 否 | 0 | ETW session 最小 buffer 数；0 表示使用 Windows 默认值；配置时最大 1024 |
| MaxBuffers | uint32 | 否 | 0 | ETW session 最大 buffer 数；0 表示使用 Windows 默认值；配置时最大 1024 |
| FlushTimerSec | uint32 | 否 | 0 | ETW buffer flush 间隔，单位秒；0 表示使用 Windows 默认值；配置时最大 60 |

> **ProviderName vs ProviderGUID**：推荐使用 `ProviderName`，插件会自动解析为 GUID，无需手动查找。当目标 Windows 系统中未注册该名称对应的 GUID 时，才需回退为 `ProviderGUID` 方式。

## 输出字段

### 通用 Header 字段

所有 ETW 事件均包含以下基础字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| event_id | string | ETW 事件 ID |
| opcode | string | 操作码 |
| level | string | 事件级别（1-5） |
| keywords | string | Keywords 位掩码（十六进制） |
| process_id | string | 产生事件的进程 ID |
| thread_id | string | 产生事件的线程 ID |

### 原始事件属性

除 Header 外，插件会透传该 ETW 事件的所有属性字段（EventProperties），并统一将字段名转换为下划线小写格式输出。具体字段取决于 ETW Provider 的事件定义。例如 `InterfaceIP` 输出为 `interface_ip`，`BufferSize` 输出为 `buffer_size`，`PacketData` 输出为 `packet_data`。

## DNS Server 字段富化

当 ProviderName 为 `Microsoft-Windows-DNSServer`（GUID: `{EB79061A-A566-4698-9119-3ED2807060E7}`）时，插件会自动对 DNS 事件进行字段富化，将原始 ETW 字段映射到 [Microsoft Sentinel ASIM DNS Schema](https://learn.microsoft.com/en-us/azure/sentinel/normalization-schema-dns)。

### DNS 原始字段

| 字段 | 说明 |
|------|------|
| qname | 查询域名（FQDN），由原始 `QNAME` 归一化 |
| qtype | 查询类型（1=A, 28=AAAA, 5=CNAME 等），由原始 `QTYPE` 归一化 |
| source | 来源 IP（客户端或上游 DNS），由原始 `Source` 归一化 |
| interface_ip | DNS 服务器监听接口 IP，由原始 `InterfaceIP` 归一化 |
| destination | 目标 IP（上游 DNS），由原始 `Destination` 归一化 |
| port | 端口号，由原始 `Port` 归一化 |
| xid | DNS 事务 ID，由原始 `XID` 归一化 |
| tcp | 协议标识，`0` 表示 UDP，`1` 表示 TCP，由原始 `TCP` 归一化 |
| packet_data | 完整 DNS 报文（十六进制编码），由原始 `PacketData` 归一化 |

### 富化输出字段

| 输出字段 | 类型 | 说明 |
|---------|------|------|
| src_ip_addr | string | 源 IP 地址（按 Event ID 从 Source 或 InterfaceIP 映射） |
| dst_ip_addr | string | 目标 IP 地址（按 Event ID 从 InterfaceIP 或 Destination 映射） |
| src_port_number | string | 源端口号（Event 256 中客户端端口） |
| dst_port_number | string | 目标端口号（Event 279 中客户端端口） |
| network_protocol | string | 网络协议，按 TCP 字段归一为 `udp` 或 `tcp` |
| dns_query | string | 查询域名（去除尾部点号） |
| dns_query_type_name | string | 查询类型名称（A, AAAA, CNAME, MX, TXT 等） |
| dns_response_code | string | DNS 响应码数字（0, 1, 2, 3 等） |
| dns_response_code_name | string | DNS 响应码名称（NOERROR, SERVFAIL, NXDOMAIN 等） |
| dns_response_name | string | DNS 应答内容（从 PacketData 解析，多条以分号分隔） |
| dns_flags | string | DNS 报文标志位（十六进制，如 0x8580） |
| event_sub_type | string | 事件子类型：`request` 或 `response` |
| event_result | string | 事件结果：`Success` 或 `Failure`（基于 RCODE） |
| event_result_details | string | DNS 响应码名称，例如 `NOERROR`、`NXDOMAIN`、`SERVFAIL`；请求事件默认 `NA` |
| dvc_ip_addr | string | DNS 服务器 IP，优先使用 InterfaceIP，必要时使用缓存的 serverIP |
| dvc_hostname | string | DNS 服务器主机名 |
| dvc_domain | string | DNS 服务器所在域，优先从 USERDNSDOMAIN 或系统 TCP/IP 参数读取 |
| dvc_domain_type | string | 域名类型：`FQDN` 或 `NetBIOS` |
| dvc_os | string | 设备操作系统，Windows 平台固定为 `Windows` |
| dvc_os_version | string | Windows 版本号，例如 `10.0.17763.0` |

### Event ID 映射规则

| Event ID | 名称 | 方向 | src_ip_addr | dst_ip_addr | 端口映射 | event_sub_type |
|----------|------|------|-----------|-----------|----------|-------------|
| 256 | QUERY_RECEIVED | 客户端→DNS服务器 | Source（客户端） | InterfaceIP（服务器） | Port → src_port_number | request |
| 260 | RECURSE_QUERY_OUT | DNS服务器→上游DNS | InterfaceIP（服务器） | Destination（上游DNS） | — | request |
| 261 | RECURSE_RESPONSE_IN | 上游DNS→DNS服务器 | Source（上游DNS） | InterfaceIP（服务器） | — | response |
| 279 | RESPONSE_TO_CLIENT | DNS服务器→客户端 | InterfaceIP（服务器） | Source（客户端） | Port → dst_port_number | response |

### serverIP 缓存机制

Event 260/261 中的 InterfaceIP 有时为 `0.0.0.0`（INADDR_ANY），无法确定 DNS 服务器真实 IP。插件通过以下策略解决：

1. Event 256 的 InterfaceIP 通常为服务器真实监听地址，当不是 `0.0.0.0` 和 `127.0.0.1` 时缓存到 `serverIP`
2. 后续 Event 260/261/279 中若 InterfaceIP 为空或 `0.0.0.0`，自动使用缓存的 `serverIP` 作为回退

### DNS 查询域名过滤

`DNSQueryDomainFilters` 可在 DNS Server 场景下按查询域名降低采集量。插件会优先使用原始 `qname` / 富化后的 `dns_query` 与配置规则匹配；配置不为空时，命中的 DNS 事件会被丢弃，未命中的 DNS 事件正常输出。

匹配规则：

- `*.example.com` 匹配 `example.com` 及其所有子域名，例如 `a.example.com`、`a.b.example.com`
- `example.com` 仅精确匹配 `example.com`
- 匹配大小写不敏感，查询域名尾部的 `.` 会自动忽略

### PacketData 解析

当事件包含 `PacketData` 字段时（十六进制编码的 DNS wire format 报文），插件使用 `github.com/miekg/dns` 库自动解析：

- 提取 dns_response_code 和 dns_response_code_name
- 提取 dns_flags（仅响应报文）
- 提取 dns_response_name（Answer 段中的 A、AAAA、CNAME、MX、NS、PTR、TXT、SOA、SRV 等记录）
- 判断 event_result（Success/Failure）
- 设置 event_result_details：响应事件使用 DNS 响应码名称，请求事件使用 `NA`

若主机 CPU 压力较高，可配置 `ParsePacketData: false` 关闭该解析。关闭后仍会保留 DNS 查询名、查询类型、源/目的地址、协议、设备字段等基础字段，但不会补齐 `dns_response_name`、`dns_flags`、`dns_response_code_name` 等响应报文解析字段。

### 异步处理与 ETW Buffer

`AsyncProcess` 可将字段归一化、Provider 专属富化、过滤和写入 Collector 的逻辑从 ETW callback 中移到 worker 处理，降低 callback 长时间阻塞导致 ETW buffer 积压的风险。由于 ETW 事件对象只在 callback 生命周期内可靠，插件仍会在 callback 中读取 `EventProperties`，但会把字符串格式化、字段名归一化、DNS 解析和 Collector 写入延后到 worker。该能力对所有 ETW Provider 生效；DNS Provider 只是在 worker 阶段额外执行 DNS 专属富化和黑名单过滤。

开启 `AsyncProcess` 后，插件会每 30 分钟输出一次统计日志，并在停止时输出最终统计，字段包括 `received`、`enqueued`、`dropped_queue_full`、`dropped_domain_filter`、`packet_data_parse_error`。如果 `DropWhenQueueFull=true` 且 `dropped_queue_full` 持续增长，说明当前处理能力低于事件产生速度，需要增大队列/worker、放宽 ETW buffer，或进一步降低采集量。

`BufferSizeKB`、`MinBuffers`、`MaxBuffers`、`FlushTimerSec` 会透传到 ETW session 的 `EVENT_TRACE_PROPERTIES`，对所有 ETW Provider 生效。这些参数主要提升突发流量下的缓冲能力，但会增加内核内存占用；如果长期处理速度低于事件产生速度，仍需通过收窄 `Keywords`、关闭 provider 专属重解析逻辑或异步处理来降低压力。对于 DNS Provider，还可以通过黑名单和 `ParsePacketData: false` 降低开销。

### 设备字段识别

插件会尽量补齐设备侧字段，便于下游按 DNS 服务器维度聚合：

- `dvc_ip_addr`：从当前事件的服务器侧 IP 识别。Event 256/261 使用 `InterfaceIP`，Event 260/279 使用服务器侧源 IP；当 `InterfaceIP` 为 `0.0.0.0` 时回退到缓存的 `serverIP`
- `dvc_hostname`：由 `os.Hostname()` 获取
- `dvc_domain` / `dvc_domain_type`：优先读取 `USERDNSDOMAIN`，其次读取 `HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters` 下的 `Domain` / `NV Domain`，最后回退到 `USERDOMAIN`
- `dvc_os` / `dvc_os_version`：Windows 平台固定输出 `Windows`，版本号从 `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion` 读取

## 配置示例

### 示例 1：采集 DNS Server Analytical 事件

```yaml
enable: true
inputs:
  - Type: service_etw
    ProviderName: "Microsoft-Windows-DNSServer"
    Level: 4
    Keywords: 0x8000000000000000
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

> Keywords 说明：`0x8000000000000000` 表示 Analytical Channel。可按需组合以下位：
> - `0x0000000000000001` — QUERY_RECEIVED
> - `0x0000000000000002` — RESPONSE_SUCCESS
> - `0x0000000000000004` — RESPONSE_FAILURE
> - `0x0000000000000010` — RECURSE_QUERY_OUT
> - `0x0000000000000020` — RECURSE_RESPONSE_IN

### 示例 2：丢弃指定 DNS 查询域名

```yaml
enable: true
inputs:
  - Type: service_etw
    ProviderName: "Microsoft-Windows-DNSServer"
    Level: 4
    Keywords: 0x8000000000000037
    DNSQueryDomainFilters:
      - "*.azure.cn"
      - "*.azure-automation.cn"
      - "*.trafficmanager.cn"
      - "*.chinacloudapi.cn"
      - "*.chinacloudapp.cn"
      - "*.reddog.microsoft.com"
      - "*.azk8s.cn"
    ParsePacketData: false
    AsyncProcess: true
    EventQueueSize: 8192
    WorkerCount: 2
    DropWhenQueueFull: true
    BufferSizeKB: 128
    MinBuffers: 8
    MaxBuffers: 64
    FlushTimerSec: 1
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

### 示例 3：采集 PowerShell 脚本执行事件

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

### 示例 4：使用 GUID 指定 Provider

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

## 常见问题

| 常见问题 | 解决方法 |
|--------|--------|
| 无事件输出 | 确认 Windows Server 2012 R2 及以上版本 |
| 权限不足 | 以管理员权限运行 loongcollector |
| Provider 找不到 | 确认 ETW Provider 名称或 GUID 正确 |
| 重复 Session | 同一 Provider 只能有一个活跃 ETW Session |
| Analytical Channel 无数据 | 确认 Keywords 包含了 Analytical Channel 对应的位 |

## 故障排查

### ETW Session 管理问题

- 插件异常退出可能残留 ETW Session
- 使用以下命令查看并清理：`logman query -ets`
- 清理残留 Session：`logman stop <session-name> -ets`

### ProviderName 无法解析

- 确认 Provider 名称拼写正确（区分大小写）
- 检查系统注册表：`reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\WINEVT\Publishers" /s /f "<ProviderName>"`

### 事件过滤不生效

- 确认 Level 和 Keywords 配置正确
- 不同 Provider 有不同的 Keywords 定义，需要查阅 Provider 文档
- 使用 `flusher_stdout` 先验证事件是否正常采集
