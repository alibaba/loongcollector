# SPL处理

## 简介

`processor_spl`插件通过SPL语句处理数据

## 版本

[Stable](../../stability-level.md)

## 配置参数

| **参数** | **类型** | **是否必填** | **默认值** | **说明** |
| --- | --- | --- | --- | --- |
| Type | string | 是 | / | 插件类型。固定为processor_spl。 |
| Script | string | 是 | / | SPL语句。日志内容默认存在content字段中。 |
| TimeoutMilliSeconds | int | 否 | 1000 | 单次SPL语句执行的超时时间。 |
| MaxMemoryBytes | int | 否 | 50 \* 1024 \* 1024 | SPL引擎可使用的最大内存。 |

## 样例

采集文件`/workspaces/ilogtail/debug/simple.log`，通过正则表达式解析日志内容并提取字段，并将结果输出到stdout。

+ 输入

```plain
127.0.0.1 - - [07/Jul/2022:10:43:30 +0800] "POST /PutData?Category=YunOsAccountOpLog" 0.024 18204 200 37 "-" "aliyun-sdk-java"
```

+ 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /workspaces/ilogtail/debug/simple.log
processors:
  - Type: processor_spl
    Script: |
      *
      | parse-regexp content, '([\d\.]+) \S+ \S+ \[(\S+) \S+\] \"(\w+) ([^\\"]*)\" ([\d\.]+) (\d+) (\d+) (\d+|-) \"([^\\"]*)\" \"([^\\"]*)\"' as ip, time, method, url, request_time, request_length, status, length, ref_url, browser
      | project-away content
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

+ 输出

```json
{
    "ip": "127.0.0.1",
    "time": "07/Jul/2022:10:43:30",
    "method": "POST",
    "url": "/PutData?Category=YunOsAccountOpLog",
    "request_time": "0.024",
    "request_length": "18204",
    "status": "200",
    "length": "37",
    "ref_url": "-",
    "browser": "aliyun-sdk-java"
}
```

更多样例可参考：[一文教会你如何使用iLogtail SPL处理日志](https://open.observability.cn/article/gpgqx50m2ry4h2mx/)
