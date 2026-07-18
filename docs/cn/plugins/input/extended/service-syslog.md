# Syslog数据

## 简介

`service_syslog` 插件在指定 **地址与端口** 上监听；LoongCollector 根据采集配置拉起该插件后，即可接收经 rsyslog 转发的系统日志、Nginx 转发的访问/错误日志，以及 syslog 客户端上报的日志。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/input/syslog/syslog.go)


## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| ✓ | — | — |

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.31 及以上

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | string，无默认值（必填） | 插件类型，固定为`service_syslog`。 |
| Address | string，`tcp://127.0.0.1:9999` | 指定本插件在 LoongCollector 进程内监听的 **协议、地址与端口**，格式为 `[tcp/udp]://[ip]:[port]`。**此处**与 rsyslog 等上游的转发目标必须一致。若运行 LoongCollector 的节点有多个可达 IP，可将地址设为 `0.0.0.0`，表示在所有网卡上监听。 |
| MaxConnections | Integer，`100` | 最大链接数，仅使用于TCP。|
| TimeoutSeconds | Integer，`0` | 在关闭远程连接之前的不活动秒数。|
| MaxMessageSize | Integer，`64 * 1024` | 通过传输协议接收的信息的最大字节数。|
| KeepAliveSeconds | Integer，`300` | 保持连接存活的秒数，仅使用于TCP。|
| ParseProtocol | string，`""` | 指定解析日志所使用的协议，默认为空，表示不解析。其中：`rfc3164`：指定使用RFC3164协议解析日志。`rfc5424`：指定使用RFC5424协议解析日志。`auto`：指定插件根据日志内容自动选择合适的解析协议。 |
| IgnoreParseFailure | bool，`true` | 指定解析失败后的操作，不配置表示放弃解析，直接填充所返回的content字段。配置为`false` ，表示解析失败时丢弃日志。 |
| AddHostname | bool，`false` | 当从/dev/log监听unixgram时，log中不包括hostname字段，所以使用rfc3164会导致解析错误，这时将AddHostname设置为`true`，就会给解析器当前主机的hostname，然后解析器就可以解析tag、program、content字段了。 |
| AutoConfigRsyslog | bool，`false` | 是否自动配置 rsyslog 转发。开启后，LoongCollector 会自动在 `/etc/rsyslog.d/` 下生成转发配置并重启 rsyslogd。仅支持 TCP 和 UDP 协议，需要 root 权限。 |
| RsyslogFilters | string 数组，不配置则默认转发所有日志（`*.*`） | 通过 rsyslog 过滤规则选择采集哪些日志。每个条目为 rsyslog 标准的 `facility.severity` 格式（如 "auth.warning"、"kern.err"）。仅在 AutoConfigRsyslog 为 true 时生效。 |

## 样例

本样例采用了udp协议监听9009端口。

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_syslog
    Address: udp://0.0.0.0:9009
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出

```json
{
    "_program_":"",
    "_facility_":"-1",
    "_hostname_":"iZb***4prZ",
    "_content_":"<78>Dec 23 15:35:01 iZb***4prZ CRON[3364]: (root) CMD (command -v ***)",
    "_ip_":"172.**.**.5",
    "_priority_":"-1",
    "_severity_":"-1",
    "_unixtimestamp_":"1640244901606136313",
    "_client_ip_":"120.**.2**.90",
    "__time__":"1640244901"
}
```

* 采集字段含义

|字段|说明|
|----|----|
|`_program_`|协议中的tag字段。|
| `_hostname_` | 主机名, 如果日志中末提供则获取当前主机名。 |
| `_program_` | 协议中的tag字段。 |
| `_priority_` | 协议中的priority字段。 |
| `_facility_` | 协议中的facility字段。 |
| `_severity_` | 协议中的severity字段。 |
| `_unixtimestamp_` | 日志对应的时间戳。 |
| `_content_` | 日志内容, 如果解析失败的话, 此字段包含末解析日志的所有内容。 |
| `_ip_` | 当前主机的IP地址。 |
|`_client_ip_`|传输日志的客户端ip地址。|

## 自动配置 rsyslog 转发

开启 `AutoConfigRsyslog` 后，LoongCollector 会在 `/etc/rsyslog.d/` 目录下自动生成 rsyslog v8+ 转发配置文件（文件名格式为 `10-loongcollector-{configName}.conf`），并在配置内容变更时自动重启 rsyslogd。

> **注意**：文件名中的 `{configName}` 会经过清洗——所有非 `[a-zA-Z0-9_-]` 的字符（如空格、`/`、`.`、`:`、中文等）都会被替换为 `_`。例如采集配置名为 `my config.a` 时，实际生成的文件为 `10-loongcollector-my_config_a.conf`。请按清洗后的名称查找文件；此外，若两个配置名清洗后相同，会指向同一个文件，需注意避免相互覆盖。

> **副作用与清理**：由于 rsyslog v8+ 的 `reload`/`SIGHUP` 不会重载配置，应用新配置只能通过 **重启 rsyslogd**（`systemctl restart rsyslog` 或 `service rsyslog restart`）实现。这是 **宿主机全局操作**，会短暂影响该主机上所有基于 rsyslog 的日志链路，且仅在配置内容确有变化时触发。当采集配置被停止或删除时，LoongCollector 会自动删除对应的转发配置文件并重启一次 rsyslogd，以避免残留的转发规则持续向已停止的端口投递并堆积磁盘队列。

**前提条件**：
- 需要 root 权限运行 LoongCollector
- 仅支持 TCP 和 UDP 协议（不支持 unixgram）
- 目标机器需安装 rsyslogd v8 及以上版本

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_syslog
    Address: tcp://127.0.0.1:9000
    AutoConfigRsyslog: true
    RsyslogFilters:
      - "auth.warning"
      - "kern.err"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

上述配置将自动在 `/etc/rsyslog.d/` 下生成仅转发 `auth.warning` 和 `kern.err` 级别日志的配置，并重启 rsyslogd 使其生效。若不配置 `RsyslogFilters`，则默认转发所有日志（`*.*`）。
