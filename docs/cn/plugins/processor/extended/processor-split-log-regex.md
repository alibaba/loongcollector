# 多行切分

## 简介

`processor_split_log_regex processor`插件实现多行日志（例如Java程序日志）的采集。

备注：建议使用[正则加速](../accelerator/regex-accelerate.md)或[分隔符加速](../accelerator/delimiter-accelerate.md)插件中的多行切分功能替代。
单独与非加速插件配合时，该插件必须设置为`processor`的第一个插件。

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数             | 类型      | 是否必选 | 说明                                                     |
| -------------- | ------- | ---- | ------------------------------------------------------ |
| Type           | String  | 是    | 插件类型                                                   |
| SplitKey       | String  | 是    | 切分依据的字段。                                               |
| SplitRegex     | String  | 是    | <p>行首正则，只有匹配上的才认为是多行日志块的行首。</p><p>默认为.*，表示每行都进行切分。</p> |
| PreserveOthers | Boolen  | 否    | 是否保留其他非SplitKey字段。                                     |
| NoKeyError     | Boolean | 否    | 无匹配的原始字段时是否报错。如果未添加该参数，则默认使用false，表示不报错。               |

## 样例

采集`/home/test-log/`路径下的`multiline.log`文件，并按行首正则进行多行切分。

* 输入

```bash
echo -e  '[2022-03-03 18:00:00] xxx1\nyyyyy\nzzzzzz\n[2022-03-03 18:00:01] xxx2\nyyyyy\nzzzzzz' >> /home/test-log/multiline.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_split_log_regex
    SplitRegex: \[\d+-\d+-\d+\s\d+:\d+:\d+]\s.*
    SplitKey: content
    PreserveOthers: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/multiline.log",
    "content": "[2022-03-03 18:00:00] xxx1\nyyyyy\nzzzzzz\n",
    "__time__": "1657367638"
}
{
    "__tag__:__path__": "/home/test-log/multiline.log",
    "content": "[2022-03-03 18:00:01] xxx2\nyyyyy\nzzzzzz",
    "__time__": "1657367638"
}
```
