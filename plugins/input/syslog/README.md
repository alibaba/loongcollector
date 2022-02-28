收集syslog信息
---

该插件通过Logtail插件对指定的地址和端口进行监听后，Logtail开始采集数据，包括通过rsyslog采集的系统日志、 Nginx转发的访问日志或错误日志，以及通过syslog客户端转发的日志。

#### 参数说明

插件类型（type）为 `service_syslog`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|type|string|是|数据源类型，固定为service_syslog。|
|Address|string|否|指定Logtail插件监听的协议、地址和端口，Logtail插件会根据Logtail采集配置进行监听并获取日志数据。格式为`[tcp/udp]://[ip]:[port]`。不配置时，默认为`tcp://127.0.0.1:9999`。注意，Logtail插件配置中设置的监听协议、地址和端口号必须与rsyslog配置文件设置的转发规则相同。如果安装Logtail的服务器有多个IP地址可接收日志，可以将地址配置为0.0.0.0，表示监听服务器的所有IP地址。|
|ParseProtocol|string|否|指定解析日志所使用的协议，默认为空，表示不解析。其中：`rfc3164`：指定使用RFC3164协议解析日志。`rfc5424`：指定使用RFC5424协议解析日志。`auto`：指定插件根据日志内容自动选择合适的解析协议。|
|IgnoreParseFailure|boolean|否|指定解析失败后的操作，不配置时，默认为`true`，表示放弃解析，直接填充所返回的content字段。配置为`false` ，表示解析失败时丢弃日志。|


#### 配置文件及结果示例

##### 插件设置示例

在`plugin.json`中:

```json
{
    "inputs":[
    {
        "detail": {
        "Address": "udp://0.0.0.0:9009"
    },
        "type": "service_syslog"
    }
    ]
}
```

本示例采用了udp协议监听9009端口。

采集结果：

```text
{"_program_":"","_facility_":"-1","_hostname_":"iZb***4prZ","_content_":"<78>Dec 23 15:35:01 iZb***4prZ CRON[3364]: (root) CMD (command -v ***)","_ip_":"172.**.**.5","_priority_":"-1","_severity_":"-1","_unixtimestamp_":"1640244901606136313","_client_ip_":"120.**.2**.90","__time__":"1640244901"}
```

采集字段含义：

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

