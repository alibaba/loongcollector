# 数据结构

本文将介绍 iLogtail 内部的数据结构以及数据流，从而大家能够对各个插件的输入输出有更加准确的理解。

## 数据类型概览

本节将介绍插件接口相关的一些数据类型，目前，iLogtail 和服务后端之间的数据类型采用了 protobuf 进行描述（[proto 文件](https://github.com/alibaba/ilogtail/blob/main/pkg/protocol/proto/sls_logs.proto)）。

### Content & LogTag

传输数据字段以及标签，简单的 key/value 对。

```protobuf
message Content
{
required string Key = 1;
required string Value = 2;
}

message LogTag
{
required string Key = 1;
required string Value = 2;
}
```

### Log

Log 是表示单条日志的数据类型，Time 字段为日志时间，Contents 字段维护了此日志的内容，由一个 key/value 列表组成。

```protobuf
message Log
{
  required uint32 Time = 1;// UNIX Time Format
  repeated Content Contents = 2;

}
```

### LogGroup

LogGroup（日志组）是对多条日志的包装：

- Logs：包含所有日志。
- Category：日志服务Logstore，可以类比Kafka 独立集群， 数据存储的独立单元。
- Topic: 日志服务Topic，一个Category（Logstore）可以划分为多个topic，不填写时Topic 为空字符串，可以类比Kafka 独立集群下的Topic概念。
- Source/MachineUUID：iLogtail 所在节点的信息，前者为 IP，后者为 UUID。
- LogTags：所有日志共同的 tag，同样由 key/value 列表组成。

```protobuf

message LogGroup
{
  repeated Log Logs = 1;
  optional string Category = 2;
  optional string Topic = 3;
  optional string Source = 4;
  optional string MachineUUID = 5;
  repeated LogTag LogTags = 6;
}
```
