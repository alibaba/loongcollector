# 键值对

## 简介

`processor_split_key_value processor`插件可以通过切分键值对的方式提取字段。

## 配置参数

| 参数                         | 类型    | 是否必选 | 说明                                                                                                                                                                        |
| ---------------------------- | ------- | -------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Type                         | String  | 是       | 插件类型。                                                                                                                                                                  |
| SourceKey                    | String  | 否       | 原始字段名。                                                                                                                                                                |
| Delimiter                    | String  | 否       | 键值对之间的分隔符。如果未添加该参数，则默认使用制表符\t。                                                                                                                  |
| Separator                    | String  | 否       | 单个键值对中键与值之间的分隔符。如果未添加该参数，则默认使用冒号（:）。                                                                                                     |
| KeepSource                   | Boolean | 否       | 是否保留原始字段。如果未添加该参数，则默认使用true，表示保留。                                                                                                              |
| ErrIfKeyIsEmpty              | Boolean | 否       | 当key为空字符串时是否告警。如果未添加该参数，则默认使用true，表示告警。                                                                                                     |
| EmptyKeyPrefix               | String  | 否       | 如果key是空字符串，可通过该参数设置key的前缀，默认为"empty_key_"，最终key的格式为前缀+序号，比如"empty_key_0"。                                                             |
| DiscardWhenSeparatorNotFound | Boolean | 否       | 无匹配的原始字段时是否丢弃该键值对。如果未添加该参数，则默认使用false，表示不丢弃。                                                                                         |
| NoSeparatorKeyPrefix         | Boolean | 否       | 无匹配的原始字段时，如果保留该键值对，可通过该参数设置key的前缀，默认为"no_separator_key_", 最终保存下来的格式为前缀+序号:报错键值对，比如"no_separator_key_0":"报错键值对" |
| ErrIfSourceKeyNotFound       | Boolean | 否       | 无匹配的原始字段时是否告警。如果未添加该参数，则默认使用true，表示告警。                                                                                                    |
| ErrIfSeparatorNotFound       | Boolean | 否       | 当指定的分隔符（Separator）不存在时是否告警。如果未添加该参数，则默认使用true，表示告警。                                                                                   |


## 样例

采集`/home/test-log/`路径下的`key_value.log`文件，并按照键值对间分隔符为制表符`\t`，键值对中的分隔符为冒号`:` 的格式进行日志解析。

* 输入

```
echo -e 'class:main\tuserid:123456\tmethod:get\tmessage:\"wrong user\"' >> /home/test-log/key_value.log
```

* 采集配置

```
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log/
    FilePattern: key_value.log
processors:
  - Type: processor_split_key_value
    SourceKey: content
    Delimiter: "\t"
    Separator: ":"
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
    "__tag__:__path__": "/home/test_log/key_value.log",
    "class": "main",
    "userid": "123456",
    "method": "get",
    "message": "\"wrong user\"",
    "__time__": "1657354602"
}
``` 