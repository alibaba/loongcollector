# processor_otel_metric插件

## 版本

[Stable](../../stability-level.md)

## 配置参数

下面是修复后的表格：

| 参数                     | 类型      | 是否必选 | 说明                                                                                                                                                                                   |
|------------------------|---------|------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SourceKey              | String  | 是    | 原始字段名。                                                                                                                                                                               |
| Format                 | String  | 是    | 转换后的格式（枚举类型）。共有三类：protobuf, json, protojson                                                                                                                                          |
| NoKeyError             | Boolean | 否    | 当没有对应字段时，是否报错。默认值为false                                                                                                                                                              |

## 样例

采集`/home/test-log`目录的`simple.log`文件，根据指定的配置选项提取日志信息。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_otel_metric
    SourceKey: "content"
    Format: protojson
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输入

```bash
echo '{ "resource": { "attributes": [] }, "scopeMetrics": [ { "scope": { "name": "proxy-meter", "attributes": [] }, "metrics": [ { "name": "rocketmq.consumer.connections", "description": "default view for gauge.", "gauge": { "dataPoints": [ { "startTimeUnixNano": "1694766421663956000", "timeUnixNano": "1694766431663946000", "asDouble": 1.0, "exemplars": [], "attributes": [ { "key": "aggregation", "value": { "stringValue": "delta" } }, { "key": "cluster", "value": { "stringValue": "serverless-rocketmq-proxy-2" } }, { "key": "consume_mode", "value": { "stringValue": "push" } }, { "key": "consumer_group", "value": { "stringValue": "group_sub4" } }, { "key": "instance_id", "value": { "stringValue": "rmq-cn-2093d0d6g05" } }, { "key": "language", "value": { "stringValue": "java" } }, { "key": "node_id", "value": { "stringValue": "serverless-rocketmq-proxy-2-546c7c9777-gnh9s" } }, { "key": "node_type", "value": { "stringValue": "proxy" } }, { "key": "protocol_type", "value": { "stringValue": "remoting" } }, { "key": "uid", "value": { "stringValue": "1936715356116916" } }, { "key": "version", "value": { "stringValue": "v4_4_4_snapshot" } } ] } ] } }, { "name": "rocketmq.rpc.latency", "histogram": { "dataPoints": [ { "startTimeUnixNano": "1694766421663956000", "timeUnixNano": "1694766431663946000", "count": "150", "sum": 14.0, "min": 0.0, "max": 1.0, "bucketCounts": [ "150", "0", "0", "0", "0", "0" ], "explicitBounds": [ 1.0, 10.0, 100.0, 1000.0, 3000.0 ], "exemplars": [], "attributes": [ { "key": "aggregation", "value": { "stringValue": "delta" } }, { "key": "cluster", "value": { "stringValue": "serverless-rocketmq-proxy-2" } }, { "key": "instance_id", "value": { "stringValue": "rmq-cn-2093d0d6g05" } }, { "key": "node_id", "value": { "stringValue": "serverless-rocketmq-proxy-2-546c7c9777-gnh9s" } }, { "key": "node_type", "value": { "stringValue": "proxy" } }, { "key": "protocol_type", "value": { "stringValue": "remoting" } }, { "key": "request_code", "value": { "stringValue": "get_consumer_list_by_group" } }, { "key": "response_code", "value": { "stringValue": "system_error" } }, { "key": "uid", "value": { "stringValue": "1936715356116916" } } ] } ], "aggregationTemporality": 1 } } ] } ] }' >> simple.log
```

* 输出

```json
[
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_consumer_connections"
      },
      {
        "Key": "__labels__",
        "Value": ""
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "1"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_sum"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "14"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_max"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "1"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_count"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "150"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_bucket"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|le#$#1|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "150"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_bucket"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|le#$#10|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "150"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_bucket"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|le#$#100|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "150"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_bucket"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|le#$#1000|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "150"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_bucket"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|le#$#3000|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "150"
      }
    ],
    "Time_ns": 663946000
  },
  {
    "Time": 1694766431,
    "Contents": [
      {
        "Key": "__name__",
        "Value": "rocketmq_rpc_latency_bucket"
      },
      {
        "Key": "__labels__",
        "Value": "aggregation#$#delta|cluster#$#serverless-rocketmq-proxy-2|instance_id#$#rmq-cn-2093d0d6g05|le#$#+Inf|node_id#$#serverless-rocketmq-proxy-2-546c7c9777-gnh9s|node_type#$#proxy|otlp_metric_aggregation_temporality#$#AGGREGATION_TEMPORALITY_DELTA|otlp_metric_histogram_type#$#Histogram|protocol_type#$#remoting|request_code#$#get_consumer_list_by_group|response_code#$#system_error|uid#$#1936715356116916"
      },
      {
        "Key": "__time_nano__",
        "Value": "1694766431663946000"
      },
      {
        "Key": "__value__",
        "Value": "150"
      }
    ],
    "Time_ns": 663946000
  }
]
```
