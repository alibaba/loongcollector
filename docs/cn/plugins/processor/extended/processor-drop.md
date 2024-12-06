# 丢弃字段

## 简介

`processor_drop processor`插件可以丢弃日志字段。

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数     | 类型     | 是否必选 | 说明                             |
| -------- | -------- | -------- | -------------------------------- |
| Type     | String   | 是       | 插件类型。                       |
| DropKeys | String[] | 是       | 指定待丢弃的字段，支持配置多个。 |

## 样例

采集`/home/test-log/`路径下的`json.log`文件，并按照`Json`格式进行日志解析，然后对解析后的字段进行丢弃。

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
  - Type: processor_drop
    DropKeys: 
      - key1
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-dir/test_log/json.log",
    "key2": "abcd",
    "__time__": "1657354602"
}
```
