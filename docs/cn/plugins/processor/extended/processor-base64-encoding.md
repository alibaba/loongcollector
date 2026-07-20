# Base64 编码

## 简介

`processor_base64_encoding`插件可以对指定字段的值进行 Base64 编码。当配置 `NewKey` 时，编码结果写入新字段；未配置时则原地覆盖 `SourceKey`。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/base64/encoding/processor_base64_encoding.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

| LogGroup(v1) | EventTypeLogging | EventTypeMetric | EventTypeSpan |
| ------------ | ---------------- | --------------- | ------------- |
|      ✅      |      ✅           |      透传        |      透传      |

> v2 场景下对每条 Log 事件的字段进行 Base64 编码（输入 Log 输出 Log），Metric/Span 事件显式透传（不处理但不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数       | 类型   | 是否必选 | 说明                                                                        |
| ---------- | ------ | -------- | --------------------------------------------------------------------------- |
| Type       | string | 是       | 插件类型，固定为 `processor_base64_encoding`。                              |
| SourceKey  | string | 是       | 待编码的原始字段名。                                                        |
| NewKey     | string | 否       | 存放编码结果的新字段名。为空时原地覆盖 `SourceKey`。                        |
| NoKeyError | bool   | 否       | 找不到 SourceKey 时是否告警。默认 `false`，表示不告警。                     |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，将字段 `content` 的内容 Base64 编码到字段 `encoded`。

* 输入

```bash
echo 'hello world' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_base64_encoding
    SourceKey: content
    NewKey: encoded
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "content": "hello world",
    "encoded": "aGVsbG8gd29ybGQ=",
    "__time__": "1657354602"
}
```
