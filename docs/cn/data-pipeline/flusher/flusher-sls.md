# SLS

## 简介

`flusher_sls` `flusher`插件可以实现将采集到的数据，经过处理后，发送到SLS。

备注：采集到SLS时，需要对`iLogtail`进行[AK、SK配置](../../configuration/system-config.md)。

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type            | String  | 是    | 插件类型，指定为`flusher_sls`。 |
| Region          | String  | 否    | 区域，一般取接入点公网服务入口前缀（需要向多个Endpoint发送时必须配置） |
| Endpoint        | String  | 是    | [SLS接入点地址](https://help.aliyun.com/document\_detail/29008.html)。 |
| ProjectName     | String  | 是    | SLS Project名。 |
| LogstoreName    | String  | 是    | SLS Logstore名。  |
| EnableShardHash | Boolean | 否    | 是否启用Key路由Shard模式写入数据。仅当配置了aggregator_shardhash时有效。如果未添加该参数，则默认使用false，表示使用负载均衡模式写入数据。 |
| KeepShardHash   | Boolean | 否    | 是否在日志tag中增加__shardhash__:&lt;shardhashkey>。仅当配置了aggregator_shardhash时有效。如果未添加该参数，则默认使用true，表示在日志中增加前述tag。 |
| ShardHashKey    | Array   | 否    | 以Key路由Shard模式写入数据时，写入shard的判定依据字段。仅当配置了加速处理插件（processor_&lt;type>_accelerate）时有效。如果未添加该参数，则默认以负载均衡模式写入数据 |

## 安全性说明

`flusher_sls` 默认使用 `HTTPS` 协议发送数据到 `SLS`，也可以使用[data_server_port](../../configuration/system-config.md)参数更高发送协议。

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到SLS。

``` yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log/
    FilePattern: "*.log"
flushers:
  - Type: flusher_sls
    Region: cn-xxx
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
```
