# GroupMetadata聚合

## 简介

`aggregator_metadata_group` `aggregator`插件可以实现对PipelineGroupEvents按照指定的 Metadata Key 进行重新聚合。仅支持v2版本。

## 版本

[Alpha](../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.4.0 及以上

## 配置参数

| 参数                  | 类型       | 是否必选 | 说明                                                           |
|---------------------|----------|------|--------------------------------------------------------------|
| Type                | String   | 是    | 插件类型，指定为`aggregator_metadata_group`。                         |
| GroupMetadataKeys   | []String | 否    | 指定需要按照其值分组的Key列表, 为空是表示丢弃Metadata, 只进行打包聚合。                  |
| GroupMaxEventLength | int      | 否    | 聚合时，单个PipelineGroupEvents中的最大Events数量，默认1024                 |
| GroupMaxByteLength  | int      | 否    | 聚合时，单个PipelineGroupEvents中Events总的字节长度，仅支持ByteArray类型，默认3MiB |
| DropOversizeEvent   | int      | 否    | 遇到单个PipelineGroupEvent字节长度超过上限是否丢弃,默认为true                   |

## 样例

采集`service_http_server`输入插件获取的字节流，使用`GroupMetadataKeys`
指定Metadata中用于聚合的字段，聚合后将采集结果通过http请求发送到指定目的。本样例中需要使用v2版本插件。

在样例中，首先通过input插件的配置`QueryParams`定义Metadata的数据来源，然后才能在aggregator插件中使用Metadata
Key进行聚合，相同`db`的Events会被放在同一个PipelineGroupEvents向后传递。在flusher_http中，每个PipelineGroupEvents中的所有Events会在一次请求中输出。

* 输入

```bash
curl --request POST 'http://127.0.0.1:12345?db=mydb' --data 'test,host=server01,region=cn value=0.1'
```

* 采集配置

```yaml
enable: true
version: "v2"
inputs:
  - Type: service_http_server
    Format: raw
    Address: "http://127.0.0.1:12345"
    QueryParams:
      - db
aggregators:
  - Type: aggregator_metadata_group
    GroupMetadataKeys:
      - db
flushers:
  - Type: flusher_http
    RemoteURL: "http://127.0.0.1:8086/write"
    Query:
      db: "%{metadata.db}"
    Convert:
      Protocol: raw
      Encoding: custom
```
