# Base64 解码

## 简介

`processor_base64_decoding`插件可以对指定字段的值进行 Base64 解码，并将解码结果写入新的字段。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/base64/decoding/processor_base64_decoding.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

> v2 场景下对每条 Log 事件的字段进行 Base64 解码（输入 Log 输出 Log），Metric/Span 事件显式透传（不处理但不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数        | 类型   | 是否必选 | 说明                                                                 |
| ----------- | ------ | -------- | -------------------------------------------------------------------- |
| Type        | string | 是       | 插件类型，固定为 `processor_base64_decoding`。                       |
| SourceKey   | string | 是       | 待解码的原始字段名。                                                 |
| NewKey      | string | 是       | 存放解码结果的新字段名。                                             |
| NoKeyError  | bool   | 否       | 找不到 SourceKey 时是否告警。默认 `false`，表示不告警。              |
| DecodeError | bool   | 否       | Base64 解码失败时是否告警。默认 `false`，表示不告警。                |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，将字段 `content` 的 Base64 内容解码到字段 `decoded`。

* 输入

```bash
echo 'aGVsbG8gd29ybGQ=' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_base64_decoding
    SourceKey: content
    NewKey: decoded
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "content": "aGVsbG8gd29ybGQ=",
    "decoded": "hello world",
    "__time__": "1657354602"
}
```
