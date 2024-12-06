# Json解析原生处理插件

## 简介

`processor_parse_json_native`插件解析事件中`Json`格式字段内容并提取新字段。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为processor\_parse\_json\_native。  |
|  SourceKey  |  string  |  是  |  /  |  源字段名。  |
|  KeepingSourceWhenParseFail  |  bool  |  否  |  false  |  当解析失败时，是否保留源字段。  |
|  KeepingSourceWhenParseSucceed  |  bool  |  否  |  false  |  当解析成功时，是否保留源字段。  |
|  RenamedSourceKey  |  string  |  否  |  空  |  当源字段被保留时，用于存储源字段的字段名。若不填，默认不改名。  |

## 样例

采集Json格式文件`/home/test-log/json.log`，解析日志内容并提取字段，并将结果输出到stdout。

- 输入

```json
{"url": "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1", "ip": "10.200.98.220", "user-agent": "aliyun-sdk-java", "request": {"status": "200", "latency": "18204"}, "time": "07/Jul/2022:10:30:28"}
```

- 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/json.log
processors:
  - Type: processor_parse_json_native
  SourceKey: content
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

- 输出

```json
{
    "__tag__:__path__": "/home/test-log/json.log",
    "url": "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1", 
    "ip": "10.200.98.220", 
    "user-agent": "aliyun-sdk-java", 
    "request": {
        "status": "200", 
        "latency": "18204"
    }, 
    "time": "07/Jul/2022:10:30:28", 
    "__time__": "1657161028"
}
```
