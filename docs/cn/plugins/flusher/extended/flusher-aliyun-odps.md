# ODPS / MaxCompute

## 简介

`flusher_aliyun_odps` 输出插件通过 ODPS Tunnel 将处理后的数据写入阿里云大数据计算服务 MaxCompute。

源代码：[flusher_odps.go](https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/odps/flusher_odps.go)

仅支持 Go v2 pipeline。采集配置必须设置 `global.StructureType: v2`，不支持 Go v1 pipeline。配置结构详见[采集配置](../../../configuration/collection-config.md)。

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| — | ✓ | v2 | v2 | v2 |

Metric 和 Span 事件转换为结构化字段后写入，字段需要与目标表的 Schema 匹配。

## 版本

[Alpha](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

| 参数 | 类型 | 是否必选 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| Type | string | 是 | 无 | 插件类型，固定为 `flusher_aliyun_odps`。 |
| Endpoint | string | 是 | 无 | MaxCompute 服务的 Endpoint，包含协议和 API 路径。 |
| AccessKeyID | string | 是 | 无 | 用于访问目标项目的 AccessKey ID。 |
| AccessKeySecret | string | 是 | 无 | 与 `AccessKeyID` 配套的 AccessKey Secret。 |
| SecurityToken | string | 否 | 空 | STS 临时凭证的 Security Token。使用 STS 时，与临时 `AccessKeyID`、`AccessKeySecret` 一起配置；使用长期 AccessKey 时不填。 |
| ProjectName | string | 是 | 无 | 目标 MaxCompute 项目名称。 |
| SchemaName | string | 否 | 空 | 目标表所属的 Schema。未设置时不覆盖 SDK 的默认 Schema。 |
| TableName | string | 是 | 无 | 目标表名称。 |
| PartitionConfig | string | 否 | 空 | 目标分区表达式，留空表示非分区表。分区表需配置，支持固定值、时间占位符和数据字段占位符，详见下文。 |
| TimeRange | int | 否 | `15` | 时间分区的对齐间隔，单位为分钟。配置了非空 `PartitionConfig` 时，必须大于等于 `15` 且为 `15` 的整数倍；未配置分区时不生效。 |

## 使用说明

目标表需要预先创建。插件读取目标表的 Schema，按列名查找事件字段并转换为目标列的类型；未匹配到输入字段的列设置为 `NULL`，多余的输入字段不写入目标表。日志字段值须为字符串或字节序列。例如，下面的文件采集样例可写入包含 `content` 字符串列的目标表。

### 非分区表

不配置 `PartitionConfig`，插件会创建不带分区键的 Tunnel 上传会话。此时无需配置 `TimeRange`。

### 分区表

通过 `PartitionConfig` 指定分区，格式如 `ds={{%Y%m%d}},hh={{%H}},mm={{%M}}`，分区列名需要与目标表一致。支持以下写法，也可以组合使用：

| 写法 | 含义 | 示例 |
| --- | --- | --- |
| 固定值 | 使用配置中的固定分区值 | `region=cn_hangzhou` |
| 时间占位符 | 使用事件时间生成分区值 | `ds={{%Y%m%d}},hh={{%H}},mm={{%M}}` |
| 数据字段占位符 | 从处理后的事件字段中读取同名字段值 | `region={{region}}` |

时间占位符中的 `%Y`、`%m`、`%d`、`%H`、`%M` 分别表示年、月、日、时、分。插件使用事件时间，先按 `TimeRange` 分钟向下对齐，再格式化分区值。例如，`TimeRange: 15` 时，事件时间 `15:17:23` 对应的分钟分区为 `15`。

数据字段占位符的字段需要在发送前准备好。例如，`ds={{%Y%m%d}},region={{region}}` 同时使用事件日期和事件中的 `region` 字段生成分区。字段不存在时，对应占位符会替换为空字符串。

## 样例

以下样例中的 `Endpoint` 和凭证均为占位值，使用时请替换为实际配置。

### 基础配置：非分区表

采集 `/home/test-log/` 路径下所有文件名匹配 `*.log` 的文件，将采集结果写入非分区表。

```yaml
enable: true
global:
  StructureType: v2
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
flushers:
  - Type: flusher_aliyun_odps
    Endpoint: https://maxcompute.example.com/api
    AccessKeyID: YOUR_ACCESS_KEY_ID
    AccessKeySecret: YOUR_ACCESS_KEY_SECRET
    ProjectName: example_project
    TableName: example_table
```

### 分区配置

写入包含 `ds`、`hh`、`mm` 分区列的表，按事件时间以 15 分钟为间隔生成分区值。

```yaml
enable: true
global:
  StructureType: v2
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
flushers:
  - Type: flusher_aliyun_odps
    Endpoint: https://maxcompute.example.com/api
    AccessKeyID: YOUR_ACCESS_KEY_ID
    AccessKeySecret: YOUR_ACCESS_KEY_SECRET
    ProjectName: example_project
    SchemaName: example_schema
    TableName: example_partitioned_table
    PartitionConfig: "ds={{%Y%m%d}},hh={{%H}},mm={{%M}}"
    TimeRange: 15
```
