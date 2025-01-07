# 重命名字段

## 简介

`processor_rename processor`插件可以将日志字段重命名。

## 支持的Event类型

| LogGroup(v1) | EventTypeLogging | EventTypeMetric | EventTypeSpan |
| ------------ | ---------------- | --------------- | ------------- |
|      ✅      |      ✅           |  ✅ 重命名Tag    | ✅ 重命名Tag   |

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数       | 类型     | 是否必选 | 说明                                                                  |
| ---------- | -------- | -------- | --------------------------------------------------------------------- |
| Type       | String   | 是       | 插件类型。                                                            |
| NoKeyError | Boolean  | 否       | 无匹配字段时是否报错。如果未添加该参数，则默认使用false，表示不报错。 |
| SourceKeys | String[] | 是       | 待重命名的原始字段。                                                  |
| DestKeys   | String[] | 是       | 重命名后的字段。                                                      |

## 样例

采集`/home/test-log/`路径下的`json.log`文件，并按照`Json`格式进行日志解析，然后对解析后的字段进行重命名。

* 输入

```bash
echo '{"key1": 123456, "key2": "abcd"}' >> /home/test-log/json.log
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
  - Type: processor_rename
    SourceKeys: 
      - key1
    DestKeys:
      - new_key1
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/key_value.log",
    "new_key1": "123456",
    "key2": "abcd",
    "__time__": "1657354602"
}
```
