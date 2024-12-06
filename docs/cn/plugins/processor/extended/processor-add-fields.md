# 添加字段

## 简介

`processor_add_fields processor`插件可以添加日志字段。

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数          | 类型    | 是否必选 | 说明                                                                 |
| ------------- | ------- | -------- | -------------------------------------------------------------------- |
| Type          | String  | 是       | 插件类型。                                                           |
| Fields        | Map     | 是       | 待添加字段和字段值。键值对格式，支持添加多个。                       |
| IgnoreIfExist | Boolean | 否       | 当Key相同时是否忽略。如果未添加该参数，则默认使用false，表示不忽略。 |

## 样例

采集`/home/test-log/`路径下的`test.log`文件，并添加字段service: A用于标识日志所在服务。

* 输入

```bash
echo 'this is a test log' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_add_fields
    Fields: 
      service: A
    IgnoreIfExist: false
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/key_value.log",
    "content": "this is a test log",
    "service": "A",
    "__time__": "1657354602"
}
```
