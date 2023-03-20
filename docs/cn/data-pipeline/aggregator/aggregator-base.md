# 基础聚合

## 简介

`aggregator_base` `aggregator`插件可以实现对单条日志的聚合。

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | String | 是 | 插件类型，指定为`aggregator_base`。 |
| MaxLogGroupCount | Int | 否 | 在执行Flush之前，内存中允许存在的最大LogGroup数量。如果未添加该参数，则默认内存中最多允许存在4个LogGroup。 |
| MaxLogCount | Int | 否 | 每个LogGroup最多可包含的日志条数。如果未添加该参数，则默认每个LogGroup最多可包含1024条日志。 |
| PackFlag | Boolean | 否 | 是否需要在LogGroup的LogTag中添加__pack_id__字段。如果未添加改参数，则默认在LogGroup的LogTag中添加__pack_id__字段。 |
| Topic | String | 否 | LogGroup的Topic名。如果未添加该参数，则默认每个LogGroup的Topic名为空。 |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，使用基础聚合功能，设定Topic为“file”，并将采集结果发送到SLS。

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log/
    FilePattern: "*.log"
aggregators:
  - Type: aggregator_base
    Topic: file
flushers:
  - Type: flusher_sls
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
```
