# 分隔符

## 简介

`processor_split_char processor`插件可以通过单字符的分隔符提取字段，该方式支持使用引用符对分隔符进行包裹。

`processor_split_string processor`插件可以通过多字符的分隔符提取字段，该方式不支持使用引用符对分隔符进行包裹。

## 版本

[Stable](../../stability-level.md)

## 配置参数

### `processor_split_char`配置

| 参数           | 类型       | 是否必选 | 说明                                       |
| ------------ | -------- | ---- | ---------------------------------------- |
| Type         | String   | 是    | 插件类型                                     |
| SourceKey    | String   | 是    | 原始字段名                                    |
| SplitSep     | String   | 是    | 分隔符。必须为单字符，可设置为不可见字符，例如\u0001。           |
| SplitKeys    | String数组 | 是    | 分割日志后设置的字段名，例如\["ip", "time", "method"]。 |
| QuoteFlag    | Boolean  | 否    | 是否使用引用符。如果未添加该参数，则默认使用false，表示不使用。       |
| Quote        | String   | 否    | 仅当QuoteFlag配置为true时有效。                   |
| NoKeyError   | Boolean  | 否    | 无匹配的原始字段时是否报错。如果未添加该参数，则默认使用false，表示不报错。 |
| NoMatchError | Boolean  | 否    | 分隔符不匹配时是否报错。如果未添加该参数，则默认使用false，表示不报错。   |
| KeepSource   | Boolean  | 否    | 是否保留原始字段。如果未添加该参数，则默认使用false，表示不保留。      |

### `processor_split_string`配置

| 参数              | 类型       | 是否必选 | 说明                                                                |
| --------------- | -------- | ---- | ----------------------------------------------------------------- |
| Type            | String   | 是    | 插件类型                                                              |
| SourceKey       | String   | 是    | 原始字段名                                                             |
| SplitSep        | String   | 是    | 分隔符。可以设置不可见字符，例如\u0001\u0002。                                     |
| SplitKeys       | String数组 | 是    | 分割日志后设置的字段名，例如\["ip", "time", "method"]。                          |
| PreserveOthers  | Boolean  | 否    | 如果待分割的字段长度大于SplitKeys参数中的字段长度时是否保留超出部分。如果未添加该参数，则默认使用false，表示不保留。 |
| ExpandOthers    | Boolean  | 否    | 是否解析超出部分。如果未添加该参数，则默认使用false，表示不继续解析。                             |
| ExpandKeyPrefix | String   | 否    | 超出部分的命名前缀。例如配置expand\_，则Key为expand\_1、expand\_2。                  |
| NoKeyError      | Boolean  | 否    | 无匹配的原始字段时是否报错。如果未添加该参数，则默认使用false，表示不报错。                          |
| NoMatchError    | Boolean  | 否    | 分隔符不匹配时是否报错。如果未添加该参数，则默认使用false，表示不报错。                            |
| KeepSource      | Boolean  | 否    | 是否保留原始字段。如果未添加该参数，则默认使用false，表示不保留。                               |

## 样例

采集`/home/test-log/`路径下的`delimiter.log`文件，使用竖线`（|）`分隔符提取日志的字段值。

* 输入

```bash
echo "127.0.0.1|10/Aug/2017:14:57:51 +0800|POST|PutData?Category=YunOsAccountOpLog|0.024|18204|200|37|-|aliyun-sdk-java" >> /home/test-log/delimiter.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_split_char
    SourceKey: content
    SplitSep: "|"
    SplitKeys:
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
    "__tag__:__path__": "/home/test-log/delimiter.log",
    "ip": "127.0.0.1",
    "time": "10/Aug/2017:14:57:51 +0800",
    "method": "POST",
    "url": "PutData?Category=YunOsAccountOpLog",
    "request_time": "0.024",
    "request_length": "18204",
    "status": "200",
    "length": "37",
    "ref_url": "-",
    "browser": "aliyun-sdk-java",
    "__time__": "1657361070"
}
```
