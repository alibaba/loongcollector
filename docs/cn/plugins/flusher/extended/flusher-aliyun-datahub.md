# DataHub

## 简介

`flusher_aliyun_datahub` 输出插件将采集到的数据发送到阿里云数据总线 DataHub，支持 Tuple 和 Blob 两种 Topic。

源代码：[flusher_datahub.go](https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/datahub/flusher_datahub.go)

仅支持 Go v2 pipeline。采集配置必须设置 `global.StructureType: v2`，不支持 Go v1 pipeline。配置结构详见[采集配置](../../../configuration/collection-config.md)。

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| — | ✓ | v2 | v2 | v2 |

日志按内容字段写入。Metric 和 Span 事件先转换为结构化字段，多值 Metric 可以生成多条记录；写入 Tuple Topic 时，需要为这些字段配置匹配的 Schema。

## 版本

[Alpha](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

| 参数 | 类型 | 是否必选 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| Type | string | 是 | 无 | 插件类型，固定为 `flusher_aliyun_datahub`。 |
| Endpoint | string | 是 | 无 | DataHub 服务地址，包含 `http://` 或 `https://` 协议前缀。 |
| AccessKeyID | string | 是 | 无 | 访问目标项目的 AccessKey ID；使用 STS 时填写临时凭证中的 AccessKey ID。 |
| AccessKeySecret | string | 是 | 无 | 与 `AccessKeyID` 配套的 AccessKey Secret。 |
| SecurityToken | string | 否 | 空 | 使用 STS 临时凭证时填写；留空时使用 AccessKey 认证。 |
| ProjectName | string | 是 | 无 | 目标 DataHub 项目名称。 |
| TopicName | string | 是 | 无 | 目标 DataHub Topic 名称。 |

## 使用说明

提前创建目标项目和 Topic，并确保配置的 Endpoint 能访问目标 DataHub 服务。插件读取 Topic 类型和 Schema，通过 `PutRecordsByShard` 接口按组批量写入 ACTIVE Shard。

### Tuple Topic

事件内容字段按名称匹配 Topic Schema，再按 Schema 类型转换。日志字段值须为字符串或字节序列；不在 Schema 中的字段、无法转换的值或不支持的目标类型会返回错误。

空字符串按空值处理：目标字段允许 NULL 时不设置该字段，否则返回错误。插件不会自动把组标签、主机信息或日志时间补入内容字段。

### Blob Topic

* 仅包含一个 `content` 字段时，直接将该字段的内容写入 Blob。
* 包含其他字段时，将内容字段编码为 JSON 对象后写入，JSON 中的字段值均为字符串。
* 没有内容字段时返回错误。

## 样例

以下 Endpoint 和凭证为占位值，使用时替换为实际值。

### 写入 Blob Topic

采集 `/home/test-log/` 下的 `*.log` 文件，将每行的原始内容写入已创建的 Blob Topic `topic_blob`。

```yaml
enable: true
global:
  StructureType: v2
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
flushers:
  - Type: flusher_aliyun_datahub
    AccessKeyID: YOUR_ACCESS_KEY_ID
    AccessKeySecret: YOUR_ACCESS_KEY_SECRET
    Endpoint: https://datahub.example.com
    ProjectName: project_sample
    TopicName: topic_blob
```

### 写入 Tuple Topic

目标 Topic `topic_tuple` 的 Schema 包含 `level` 和 `message` 两个 STRING 字段。

* 输入

```json
{"level":"INFO","message":"hello DataHub"}
```

* 采集配置

通过 JSON 解析提取内容字段，并移除原始 `content` 字段，使写入字段与 Topic Schema 匹配。

```yaml
enable: true
global:
  StructureType: v2
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_parse_json_native
    SourceKey: content
    KeepingSourceWhenParseSucceed: false
flushers:
  - Type: flusher_aliyun_datahub
    AccessKeyID: YOUR_ACCESS_KEY_ID
    AccessKeySecret: YOUR_ACCESS_KEY_SECRET
    Endpoint: https://datahub.example.com
    ProjectName: project_sample
    TopicName: topic_tuple
```
