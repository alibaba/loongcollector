# 正则

## 简介

`processor_regex processor`插件可以通过正则匹配的模式实现文本日志的字段提取。

备注：当作为第一个日志解析插件时，建议使用[正则加速](../accelerator/regex-accelerate.md)插件替代。

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数           | 类型       | 是否必选 | 说明                                                                        |
| ------------ | -------- | ---- | ------------------------------------------------------------------------- |
| Type         | String   | 是    | 插件类型                                                                      |
| SourceKey    | String   | 是    | 原始字段名                                                                     |
| Regex        | String   | 是    | 正则表达式，使用()标注待提取的字段。                                                       |
| Keys         | String数组 | 是    | 提取的字段名，例如\["ip", "time", "method"]。                                       |
| NoKeyError   | Boolean  | 否    | 无匹配的原始字段时是否报错。如果未添加该参数，则默认使用false，表示不报错。                                  |
| NoMatchError | Boolean  | 否    | 正则表达式与原始字段的值不匹配时是否报错。如果未添加该参数，则默认使用false，表示不报错。                           |
| KeepSource   | Boolean  | 否    | 是否保留原始字段。如果未添加该参数，则默认使用false，表示不保留。                                       |
| FullMatch    | Boolean  | 否    | 如果未添加该参数，则默认使用true，表示只有字段完全匹配Regex参数中的正则表达式时才被提取。配置为false，表示部分字段匹配也会进行提取。 |
| KeepSourceIfParseError | Boolean | 否    | 解析失败时，是否保留原始日志。如果未添加该参数，则默认使用true，表示保留原始日志。       |

## 样例

采集`/home/test-log/`路径下的`reg.log`文件，日志内容按照提取字段。

* 输入

```bash
echo '127.0.0.1 - - [10/Aug/2017:14:57:51 +0800] "POST /PutData?Category=YunOsAccountOpLog" 0.024 18204 200 37 "-" "aliyun-sdk-java"' >> /home/test-log/reg.log
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

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/reg.log",
    "ip": "127.0.0.1",
    "time": "10/Aug/2017:14:57:51",
    "method": "POST",
    "url": "/PutData?Category=YunOsAccountOpLog",
    "request_time": "0.024",
    "request_length": "18204",
    "status": "200",
    "length": "37",
    "ref_url": "-",
    "browser": "aliyun-sdk-java",
    "__time__": "1657362166"
}
```
