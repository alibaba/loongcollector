# 过滤原生处理插件

## 简介

`processor_filter_regex_native`插件根据事件字段内容来过滤事件。

## 版本

[Stable](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为processor\_filter\_regex\_native。  |
|  FilterKey  |  \[string\]  |  是  |  /  |  过滤字段名，需配套`FilterRegex`参数使用，表示如果当前事件要被采集，则key指定的字段内容所需要满足的条件。多个条件之间为“且”的关系，仅当所有条件均满足时，该条日志才会被采集。  |
|  FilterRegex  |  \[string\]  |  是  |  /  |  与`FilterKey`对应的过滤正则表达式。必须与`FilterKey`长度相同。  |

## 样例

采集文件`/home/test-log/reg.log`，通过正则表达式解析日志内容并提取字段，然后只采集method为POST或PUT且状态码为200的日志，并将结果输出到stdout。

- 输入

```plain
127.0.0.1 - - [07/Jul/2022:10:43:30 +0800] "POST /PutData?Category=YunOsAccountOpLog" 0.024 18204 200 37 "-" "aliyun-sdk-java"
127.0.0.1 - - [07/Jul/2022:10:44:30 +0800] "Get /PutData?Category=YunOsAccountOpLog" 0.024 18204 200 37 "-" "aliyun-sdk-java"
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
  - Type: processor_filter_regex_native
    FilterKey: 
      - method
      - status
    FilterRegex:
      - ^(POST|PUT)$
      - ^200$
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
