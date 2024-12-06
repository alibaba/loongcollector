# 正则解析原生处理插件

## 简介

`processor_parse_regex_native`插件通过正则匹配解析事件指定字段内容并提取新字段

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为processor\_parse\_regex\_native。  |
|  SourceKey  |  string  |  是  |  /  |  源字段名。  |
|  Regex  |  string  |  是  |  /  |  正则表达式。  |
|  Keys  |  \[string\]  |  是  |  /  |  提取的字段列表。  |
|  KeepingSourceWhenParseFail  |  bool  |  否  |  false  |  当解析失败时，是否保留源字段。  |
|  KeepingSourceWhenParseSucceed  |  bool  |  否  |  false  |  当解析成功时，是否保留源字段。  |
|  RenamedSourceKey  |  string  |  否  |  空  |  当源字段被保留时，用于存储源字段的字段名。若不填，默认不改名。  |

## 样例

采集文件`/home/test-log/reg.log`，通过正则表达式解析日志内容并提取字段，并将结果输出到stdout。

- 输入

```plain
127.0.0.1 - - [07/Jul/2022:10:43:30 +0800] "POST /PutData?Category=YunOsAccountOpLog" 0.024 18204 200 37 "-" "aliyun-sdk-java"
```

- 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/reg.log
processors:
  - Type: processor_parse_regex_native
    SourceKey: content
    Regex: ([\d\.]+) \S+ \S+ \[(\S+) \S+\] \"(\w+) ([^\\"]*)\" ([\d\.]+) (\d+) (\d+) (\d+|-) \"([^\\"]*)\" \"([^\\"]*)\"
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
    "__tag__:__path__": "/home/test-log/reg.log",
    "ip": "127.0.0.1",
    "time": "07/Jul/2022:10:43:30",
    "method": "POST",
    "url": "/PutData?Category=YunOsAccountOpLog",
    "request_time": "0.024",
    "request_length": "18204",
    "status": "200",
    "length": "37",
    "ref_url": "-",
    "browser": "aliyun-sdk-java",
    "__time__": "1657161810"
}
```
