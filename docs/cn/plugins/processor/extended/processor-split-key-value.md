# 键值对

## 简介

`processor_split_key_value processor`插件可以通过切分键值对的方式提取字段。

## 版本

[Stable](../../stability-level.md)

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
| Quote                        | String | 否       | 引用符，当设定后若值被引用符包含，就提取引用符内的值。<br>注意引用符若为双引号，需要加转义符\。<br>当引用符内包含\字符与引用连用的情况，作为值的一部分输出。<br>引用符支持多字符。<br>默认不开启引用符功能。  |

## 样例

### 切分键值对1

采集`/home/test-log/`路径下的`key_value.log`文件，并按照键值对间分隔符为制表符`\t`，键值对中的分隔符为冒号`:` 的格式进行日志解析。

* 输入

```bash
echo -e 'class:main\tuserid:123456\tmethod:get\tmessage:\"wrong user\"' >> /home/test-log/key_value.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_split_key_value
    SourceKey: content
    Delimiter: "\t"
    Separator: ":"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/key_value.log",
    "class": "main",
    "userid": "123456",
    "method": "get",
    "message": "\"wrong user\"",
    "__time__": "1657354602"
}
```

### 包含引用符的切分键值对

采集`/home/test-log/`路径下的`key_value.log`文件，并按照键值对间分隔符为制表符`\t`，键值对中的分隔符为冒号`:` 的格式进行日志解析。

* 输入

```bash
echo -e 'class:main http_user_agent:"User Agent" "中文" "hello\t\"ilogtail\"\tworld"' >> /home/test-log/key_value.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_split_key_value
    SourceKey: content
    Delimiter: " "
    Separator: ":"
    Quote: "\""
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/key_value.log",
    "class": "main",
    "http_user_agent": "User Agent",
    "no_separator_key_0": "中文",
    "no_separator_key_1": "hello\t\"ilogtail\"\tworld",
    "__time__": "1657354602"
}
```

### 包含多字符引用符的切分键值对

采集`/home/test-log/`路径下的`key_value.log`文件，并按照键值对间分隔符为制表符`\t`，键值对中的分隔符为冒号`:` 的格式进行日志解析。

* 输入

```bash
echo -e 'class:main http_user_agent:"""User Agent""" """中文"""' >> /home/test-log/key_value.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_split_key_value
    SourceKey: content
    Delimiter: " "
    Separator: ":"
    Quote: "\"\"\""
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/key_value.log",
    "class": "main",
    "http_user_agent": "User Agent",
    "no_separator_key_0": "中文",
    "__time__": "1657354602"
}
```
