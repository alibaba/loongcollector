# 日志限速

## 简介

`processor_rate_limit processor`插件用于对日志进行限速处理，确保在设定的时间窗口内，具有相同索引值的日志条目的数量不超过预定的速率限制。若某索引值下的日志条目数量超出限定速率，则超额的日志条目将被丢弃不予采集。
以"ip"字段作为索引的情况为例，考虑两条日志：`{"ip": "10.**.**.**", "method": "POST", "browser": "aliyun-sdk-java"}` 和`{"ip": "10.**.**.**", "method": "GET", "browser": "aliyun-sdk-c++"}`。这两条日志有相同的"ip"索引值（即 "10..."）。在此情形下，系统将对所有"ip"为"10..."的日志条目进行累计，确保其数量在限定时间窗口内不超过设定的速率限制。

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数                     | 类型，默认值 | 说明                                                |
| ---------------------- | ------- | ------------------------------------------------- |
| Fields                | []string，`[]` | 限速的索引字段。processor会根据这些字段的值所组合得到的结果，进行分别限速。|
| Limit                | string，`[]` | 限速速率。格式为 `数字/时间单位`。支持的时间单位为 `s`（每秒），`m`（每分钟），`h`（每小时）
|

## 样例

* 输入

```bash
echo '{"ip": "10.**.**.**", "method": "POST", "brower": "aliyun-sdk-java"}' >> /home/test-log/proccessor-rate-limit.log
echo '{"ip": "10.**.**.**", "method": "POST", "brower": "aliyun-sdk-java"}' >> /home/test-log/proccessor-rate-limit.log
echo '{"ip": "10.**.**.**", "method": "POST", "brower": "aliyun-sdk-java"}' >> /home/test-log/proccessor-rate-limit.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_json
    SourceKey: content
    KeepSource: false
    ExpandDepth: 1
    ExpandConnector: ""
  - Type: processor_rate_limit
    Fields:
      - "ip"
    Limit: "1/s"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
  "__tag__:__path__": "/home/test-log/proccessor-rate-limit.log",
  "__time__": "1658837955",
  "brower": "aliyun-sdk-java",
  "ip": "10.**.**.**",
  "method": "POST"
}
```
