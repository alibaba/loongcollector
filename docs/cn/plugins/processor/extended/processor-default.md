# 原始数据

## 简介

`processor_default`插件不对数据任何操作，只是简单的数据透传。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/defaultone/processor_default.go)

## 支持的Event类型

| LogGroup(v1) | EventTypeLogging | EventTypeMetric | EventTypeSpan |
| ------------ | ---------------- | --------------- | ------------- |
|      ✅      |      ✅           |       ✅        |       ✅      |

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type    | String，无默认值（必填） | 插件类型，固定为`processor_default`      |

## 样例

采集`/home/test-log/`路径下的`default.log`文件，提取文件的原始数据。

* 输入

```bash
echo "2022/07/14/11:32:47 test log" >> /home/test-log/default.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_default
    SourceKey: content
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__":"/home/test-log/default.log",
    "content":"2022/07/14/11:32:47 test log",
    "__time__":"1657769827"}
}
```
