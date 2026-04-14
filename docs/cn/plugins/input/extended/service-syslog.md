# Syslog数据

## 简介

`service_syslog` 插件在指定 **地址与端口** 上监听；LoongCollector 根据采集配置拉起该插件后，即可接收经 rsyslog 转发的系统日志、Nginx 转发的访问/错误日志，以及 syslog 客户端上报的日志。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/input/syslog/syslog.go)

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
