# Json

## 简介

`processor_json processor`插件可以实现对`Json`格式日志的解析。

## 配置参数

| 参数                     | 类型      | 是否必选 | 说明                                                |
| ---------------------- | ------- | ---- | ------------------------------------------------- |
| Type                   | String  | 是    | 插件类型                                              |
| SourceKey              | String  | 是    | 原始字段名                                             |
| NoKeyError             | Boolean | 否    | 无匹配字段时是否报错。如果未添加该参数，则默认使用true，表示报错。               |
| ExpandDepth            | Int     | 否    | JSON展开的深度。如果未添加该参数，则默认为0，表示不限制。1表示当前层级，以此类推。      |
| ExpandConnector        | String  | 否    | JSON展开时的连接符，可以为空。如果未添加该参数，则默认使用下划线（\_）。           |
| Prefix                 | String  | 否    | JSON展开时对字段附加的前缀。如果未添加该参数，则默认为空。                   |
| KeepSource             | Boolean | 否    | 是否保留原始字段。如果未添加该参数，则默认使用true，表示保留。                 |
| UseSourceKeyAsPrefix   | Boolean | 否    | 是否将原始字段名作为所有JSON展开字段名的前缀。如果未添加该参数，则默认使用false，表示否。 |
| KeepSourceIfParseError | Boolean | 否    | 解析失败时，是否保留原始日志。如果未添加该参数，则默认使用true，表示保留原始日志。       |

## 样例

采集`/home/test-log/`路径下的`json.log`文件，并按照`Json`格式进行日志解析。

* 输入

```
echo '{"key1": 123456, "key2": "abcd"}' >> /home/test-log/json.log
```

* 采集配置

```
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log/
    FilePattern: json.log
processors:
  - Type: processor_json
    SourceKey: content
    KeepSource: false
    ExpandDepth: 1
    ExpandConnector: ""
flushers:
  - Type: flusher_sls
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```
{
    "__tag__:__path__": "/home/test-dir/test_log/json.log",
    "key1": "123456",
    "key2": "abcd",
    "__time__": "1657354602"
}
```
