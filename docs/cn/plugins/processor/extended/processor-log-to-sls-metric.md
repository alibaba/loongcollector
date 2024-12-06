# 日志转换为sls metric

## 简介

`processor_log_to_sls_metric`插件可以根据配置将日志转换为sls metric。

## 版本

[Beta](../../stability-level.md)

## 配置参数

| 参数                 | 类型     | 必选或可选 | 参数说明                                                                                                                                                                |
|--------------------|--------|-------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| MetricTimeKey      | String | 可选    | 指定要用作时间戳 `__time_nano__` 的字段。默认取`log.Time`。确保指定的字段是合法的、符合格式的时间戳，目前支持second(秒，10位长度)、millisecond(毫秒，13位长度)、microsecond(微秒，16位长度)、nanosecond(纳秒，19位长度) 为单位的 Unix 时间戳。 |
| MetricLabelKeys    | Array  | 必选    | `__labels__` 字段的key列表，key需遵循正则表达式： `^[a-zA-Z_][a-zA-Z0-9_]*$`，不能包含`__labels__`。如果原始字段中存在 `__labels__` 字段，该值将被追加到列表中。 Label的Value不能包含竖线（\|）和 "#$#"。                  |
| MetricValues       | Map    | 必选    | 时序字段名所使用的key与时序值使用的key的映射。name需遵循正则表达式：`^[a-zA-Z_:][a-zA-Z0-9_:]*$` ，时序值需要是double类型的字符串。                                                                            |
| CustomMetricLabels | Map    | 可选    | 要追加的自定义 `__labels__` 字段。 key需遵循正则表达式： `^[a-zA-Z_][a-zA-Z0-9_]*$`，Value不能包含竖线（\|）和 "#$#"。                                                                            |
| IgnoreError        | Bool   | 可选    | 当日志没有匹配时是否输出Error日志。如果未添加该参数，则默认使用false，表示不忽略。                                                                                                                      |

注意：MetricTimeKey、MetricLabelKeys、MetricValues、CustomMetricLabels的字段不可相互重复。

## 样例

以下是一个示例配置，展示了如何使用 `processor_log_to_sls_metric` 插件来处理数据：

采集`/home/test-log/`路径下的`nginx.log`文件，首先使用`processor_regex`插件提取log内容，然后测试`processor_log_to_sls_metric`的功能。

* 输入

```bash
echo '::1 - - [18/Jul/2022:07:28:01 +0000] "GET /hello/ilogtail HTTP/1.1" 404 153 "-" "curl/7.74.0" "-"' >> /home/test-log/nginx.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: '([\d\.:]+) - (\S+) \[(\S+) \S+\] \"(\S+) (\S+) ([^\\"]+)\" (\d+) (\d+) \"([^\\"]*)\" \"([^\\"]*)\" \"([^\\"]*)\"'
    Keys:
      - remote_addr
      - remote_user
      - time_local
      - method
      - url
      - protocol
      - status
      - body_bytes_sent
      - http_referer
      - http_user_agent
      - http_x_forwarded_for
  - Type: processor_log_to_sls_metric
    MetricLabelKeys:
      - url
      - method
    MetricValues:
      remote_addr: status
    CustomMetricLabels:
      nginx: test
    IgnoreError: false
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
  "__labels__":"method#$#GET|nginx#$#test|url#$#/hello/ilogtail",
  "__name__":"::1",
  "__value__":"404",
  "__time_nano__":"1688956340000000000",
  "__time__":"1688956340"
}
```
