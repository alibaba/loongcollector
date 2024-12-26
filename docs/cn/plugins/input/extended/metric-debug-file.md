# 文本日志（debug）

## 简介

`metric_debug_file` 插件是一个用于调试的插件，它可以逐行读取指定文件的内容，并将其与指定的名称绑定在一起成为一个字段，用于模拟文件输入。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/input/debugfile/input_debug_file.go)

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数                | 类型      | 是否必选 | 说明                                                                         |
| ----------------- | ------- | ---- | -------------------------------------------------------------------------- |
| Type              | String  | 是    | 插件类型，固定为`metric_debug_file`。                                           |
| InputFilePath     | String  | 是    | 要读入的文件路径。                      |
| FieldName         | String  | 否    | 生成的字段的字段名。默认取值为`content`。                   |
| LineLimit         | Integer | 否    | 读取文件的前n行。默认取值为1000行。 |

## 样例

* 输入

```shell
echo "2022/07/26/14:25:47 hello world" >> /home/test-log/debug_file.log
echo "2023/05/23/19:37:47 hello world" >> /home/test-log/debug_file.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: metric_debug_file
    InputFilePath: /home/test-log/debug_file.log
    FieldName: HelloWorld
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出

```json
{
    "HelloWorld": "2022/07/26/14:25:47 hello world",
    "__time__":"1658814799"
}
{
    "HelloWorld": "2023/05/23/19:37:47 hello world",
    "__time__":"1658814799"
}
```
