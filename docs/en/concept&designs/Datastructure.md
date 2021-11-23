# Data structure
This article will introduce the internal data structure and data flow of iLogtail.

## Overview of data types
This section will introduce some data types related to the plug-in interface. Currently, the data types between iLogtail and the service backend are described by [protobuf]( ../../../pkg/protocol/proto/sls_logs.proto) .

### Content & LogTag
Transfer data fields and labels, simple key/value pairs.

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
Log is the data type that represents a single log. The Time field is the log time. The Contents field maintains the content of this log and consists of a key/value list.

```protobuf
message Log
{
  required uint32 Time = 1;// UNIX Time Format
  repeated Content Contents = 2;

}
```

### LogGroup

LogGroup (log group) is a package of multiple logs:

- Logs: Contains all logs.
- Category: The Logstore of SLS, which is the independent unit of data storage and can be compared to Kafka independent cluster.
- Topic: Topic of the log service. A Category (Logstore) can be divided into multiple topics. If it is not filled in, Topic is an empty string, which can be compared to the concept of Topic of Kafka.
- Source/MachineUUID: The information of the node where iLogtail is located. The former is the IP and the latter is the UUID.
- LogTags: The tags to all logs, which also consists of a key/value list.

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

