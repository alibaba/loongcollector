# MD5

## 简介

`processor_md5`插件可以计算指定字段值的 MD5，并将结果写入新的字段。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/md5/processor_md5.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

> v2 场景下对每条 Log 事件计算字段 MD5（输入 Log 输出 Log），Metric/Span 事件显式透传（不处理但不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数       | 类型   | 是否必选 | 说明                                                              |
| ---------- | ------ | -------- | --------------------------------------------------------------- |
| Type       | string | 是       | 插件类型，固定为 `processor_md5`。                              |
| SourceKey  | string | 是       | 待计算 MD5 的原始字段名。                                       |
| MD5Key     | string | 是       | 存放 MD5 结果的新字段名。                                       |
| NoKeyError | bool   | 否       | 找不到 SourceKey 时是否告警。默认 `false`，表示不告警。         |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，计算字段 `content` 的 MD5 并写入字段 `content_md5`。

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
  - Type: processor_md5
    SourceKey: content
    MD5Key: content_md5
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "content": "hello world",
    "content_md5": "5eb63bbbe01eeed093cb22bb8f5acdc3",
    "__time__": "1657354602"
}
```
