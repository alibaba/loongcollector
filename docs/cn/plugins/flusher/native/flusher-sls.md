# SLS

## 简介

`flusher_sls` `flusher`插件将采集到的事件发送至SLS。

使用本插件时，必须在启动参数中配置[AK和SK](../../../configuration/system-config.md)。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为 `flusher_sls`。  |
|  Project  |  string  |  是  |  /  |  Project 名称。  |
|  Logstore  |  string  |  条件必填  |  /  |  Logstore 名称。`TelemetryType` 为 `logs`/`metrics`/`metrics_multivalue` 时必填。 |
|  Region  |  string  |  否  |  进程默认地域 |  Project 所在区域。未显式配置时使用默认地域。 |
|  Endpoint  |  string  |  社区版必填  |  /  |  [SLS 接入点地址](https://help.aliyun.com/document_detail/29008.html)。企业版可由配置中心下发。 |
|  TelemetryType  |  enum  |  否  |  `logs` | 可选：`logs`、`metrics`、`metrics_multivalue`、`metrics_host`、`arms_agentinfo`、`arms_metrics`、`arms_traces`。不同取值对应不同上报通道。 |
|  Workspace  |  string  |  否  |  /  |  APM/ARMS 模式下需要提供的工作空间。 |
|  ShardHashKeys  |  []string  |  否  |  /  |  分片哈希字段列表，仅 `logs` 且未开启 Exactly Once 时生效。 |
|  Batch  |  object  |  否  |  /  |  批处理选项（包含历史的 `Batch.ShardHashKeys` 已废弃）。 |
|  CompressType  |  enum  |  否  |  `lz4`  |  是否开启压缩及压缩算法。 |
|  MaxSendRate  |  uint  |  否  |  不限速  |  单队列最大发送速率（字节/秒），仅开启 Exactly Once 时生效。 |

## 路由与安全性

如需根据事件类型或标签将不同数据路由到不同 `flusher_sls`，请使用独立组件 [路由](router.md)（在 `flushers` 列表中为某些 flusher 配置 `Match`，由 Router 负责生效）。

`flusher_sls` 默认使用 `HTTPS` 协议发送数据到 SLS，也可以使用 `data_server_port` 参数更改发送协议。

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到SLS。

``` yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_sls
    Region: cn-hangzhou
    Endpoint: cn-hangzhou.log.aliyuncs.com
    Project: test_project
    Logstore: test_logstore
```
