# 分隔符解析原生处理插件

## 简介

`processor_parse_delimiter_native`插件解析事件中分隔符格式字段内容并提取新字段。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为processor\_parse\_delimiter\_native。  |
|  SourceKey  |  string  |  是  |  /  |  源字段名。  |
|  Separator  |  string  |  是  |  /  |  分隔符。  |
|  Quote  |  string  |  否  |  "  |  引用符。  |
|  Keys  |  \[string\]  |  是  |  /  |  提取的字段列表。  |
|  AllowingShortenedFields  |  bool  |  否  |  true  |  是否允许提取的字段数量小于Keys的数量。若不允许，则此情景会被视为解析失败。  |
|  OverflowedFieldsTreatment  |  string  |  否  |  extend  |  当提取的字段数量大于Keys的数量时的行为。可选值包括：<ul><li>extend：保留多余的字段，且每个多余的字段都作为单独的一个字段加入日志，多余字段的字段名为\_\_column$i\_\_，其中$i代表额外字段序号，从0开始计数。</li><li>keep：保留多余的字段，但将多余内容作为一个整体字段加入日志，字段名为\_\_column0\_\_.</li><li>discard：丢弃多余的字段。</li></ul>  |
|  KeepingSourceWhenParseFail  |  bool  |  否  |  false  |  当解析失败时，是否保留源字段。  |
|  KeepingSourceWhenParseSucceed  |  bool  |  否  |  false  |  当解析成功时，是否保留源字段。  |
|  RenamedSourceKey  |  string  |  否  |  空  |  当源字段被保留时，用于存储源字段的字段名。若不填，默认不改名。  |

## 样例

采集分隔符格式文件`/home/test-log/delimiter.log`，解析日志内容并提取字段，并将结果输出到stdout。

- 输入

```plain
127.0.0.1,07/Jul/2022:10:43:30 +0800,POST,PutData?Category=YunOsAccountOpLog,0.024,18204,200,37,-,aliyun-sdk-java
```

- 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/json.log
processors:
  - Type: processor_parse_delimiter_native
    SourceKey: content
    Separator: ','
    Quote: '"'
    Keys:
      - ip
      - time
      - method
      - url
      - request_time
      - request_length
      - status
      - length
      - ref_url
      - browser
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

- 输出

```json
{
    "__tag__:__path__": "/home/test-log/json.log",
    "ip": "127.0.0.1",
    "time": "07/Jul/2022:10:43:30 +0800",
    "method": "POST",
    "url": "PutData?Category=YunOsAccountOpLog",
    "request_time": "0.024",
    "request_length": "18204",
    "status": "200",
    "length": "37",
    "ref_url": "-",
    "browser": "aliyun-sdk-java",
    "__time__": "1657161028"
}
```
