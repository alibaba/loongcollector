# processor_otel_trace插件

## 版本

[Stable](../../stability-level.md)

## 配置参数

下面是修复后的表格：

| 参数                     | 类型      | 是否必选 | 说明                                                                                                                                                                                   |
|------------------------|---------|------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SourceKey              | String  | 是    | 原始字段名。                                                                                                                                                                               |
| Format                 | String  | 是    | 转换后的格式（枚举类型）。共有三类：protobuf, json, protojson                                                                                                                                          |
| NoKeyError             | Boolean | 否    | 当没有对应字段时，是否报错。默认值为false                                                                                                                                                              |
| TraceIDNeedDecode      | Boolean | 否    | 是否对TraceID做一次Base64 Encode。默认值为false，当Format为protojson时，需要考虑将当前值改为True。具体原因如下：protojson转换时将TraceID数据Base64 Decode转码，而ProtoJson格式存在没有对TraceID做Base64 Encode转码的情况，这样会导致转换失败。           |
| SpanIDNeedDecode       | Boolean | 否    | 是否对SpanID做一次Base64 Encode。默认值为false，当Format为protojson时，需要考虑将当前值改为True。具体原因如下：protojson转换时将SpanID数据Base64 Decode转码，而ProtoJson格式存在没有对TraceID做Base64 Encode转码的情况，这样会导致转换失败。             |
| ParentSpanIDNeedDecode | Boolean | 否    | 是否对ParentSpanID做一次Base64 Encode。默认值为false，当Format为protojson时，需要考虑将当前值改为True。具体原因如下：protojson转换时将ParentSpanID数据Base64 Decode转码，而ProtoJson格式存在没有对TraceID做Base64 Encode转码的情况，这样会导致转换失败。 |

## 样例

采集`/home/test-log`目录下的`simple.log`文件，根据指定的配置选项提取日志信息。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_otel_trace
    SourceKey: "content"
    Format: protojson
    TraceIDNeedDecode: true
    SpanIDNeedDecode: true
    ParentSpanIDNeedDecode: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输入

```bash
echo '{ "resource": {}, "scopeSpans": [ { "scope": { "name": "com.alibaba.cloud", "attributes": [] }, "spans": [ { "traceId": "31646461386336653337343330356530", "spanId": "0108b2d29b652107", "parentSpanId": "468e99f19f43d0db", "name": "QueryExecutor::localQuery()", "kind": 1, "startTimeUnixNano": "1689831889338531120", "endTimeUnixNano": "1689831889338737020", "attributes": [ { "key": "query.early_stop.pe_query_cnt", "value": { "stringValue": "1" } }, { "key": "query.early_stop.rt_query_cnt", "value": { "stringValue": "0" } }, { "key": "query.visit_pe_num", "value": { "stringValue": "1" } }, { "key": "query.early_stop.total_limit", "value": { "stringValue": "2" } }, { "key": "query.early_stop.hits", "value": { "stringValue": "2" } } ], "events": [], "links": [], "status": {} } ] } ], "schemaUrl": "https://opentelemetry.io/schemas/1.20.0" }' >> simple.log
```

* 输出

```json
{
  "resource": {},
  "scopeSpans": [
    {
      "scope": {
        "name": "com.alibaba.cloud",
        "attributes": []
      },
      "spans": [
        {
          "traceId": "31646461386336653337343330356530",
          "spanId": "0108b2d29b652107",
          "parentSpanId": "468e99f19f43d0db",
          "name": "QueryExecutor::localQuery()",
          "kind": 1,
          "startTimeUnixNano": "1689831889338531120",
          "endTimeUnixNano": "1689831889338737020",
          "attributes": [
            {
              "key": "query.early_stop.pe_query_cnt",
              "value": {
                "stringValue": "1"
              }
            },
            {
              "key": "query.early_stop.rt_query_cnt",
              "value": {
                "stringValue": "0"
              }
            },
            {
              "key": "query.visit_pe_num",
              "value": {
                "stringValue": "1"
              }
            },
            {
              "key": "query.early_stop.total_limit",
              "value": {
                "stringValue": "2"
              }
            },
            {
              "key": "query.early_stop.hits",
              "value": {
                "stringValue": "2"
              }
            }
          ],
          "events": [],
          "links": [],
          "status": {}
        }
      ]
    }
  ],
  "schemaUrl": "https://opentelemetry.io/schemas/1.20.0"
}
```
