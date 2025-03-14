# Datahub

## 简介

`flusher_datahub` `flusher`插件可以实现将采集到的数据，经过处理后，发送到阿里云数据总线[Datahub](https://dhsnext.console.aliyun.com/cn-hangzhou/overview)。

## 版本

[Deprecated](../../stability-level.md)

## 版本说明

* 推荐版本：无

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                 |
|-------------------------------------|----------|------|------------------------------------------------------------------------------------|
| Type                                | String    | 是    | 插件类型，固定为`flusher_datahub`
| Endpoint                            | String    | 是    | 数据总线 Datahub 的域名 |
| AccessKeyId                         | String    | 是    | 阿里云账号下的 AccessKeyId |
| AccessKeySecret                     | String    | 是    | 阿里云账号下的 AccessKeySecret，与上述 AccessKeyId 配合使用 |
| ProjectName                         | String    | 是    | 数据总线 Datahub 项目名称，可在控制台中创建和查询 |
| TopicName                           | String    | 是    | 数据总线 Datahub 项目名称，可在控制台中创建和查询 |

## 说明

LoongCollector 会将采集到的数据会直接通过 PutRecord 接口写入到 Datahub 中。

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 Datahub，每个文件的大小设置为100K。这里我们假设待采集的文件的数据写入量远大于100K/hour。

```yaml
enable: true
inputs:
    - FilePaths:
        - /home/test-log/*.log
      Type: input_file
flushers:
    - Type: flusher_datahub
      AccessKeyId: xxxxx
      AccessKeySecret: xxxxx
      Endpoint: http://dh-cn-hangzhou.aliyun-inc.com
      ProjectName: project_sample
      TopicName: topic_sample
```
可以在阿里云数据总线Datahub控制台上看到已采集到的数据。[Datahub控制台](https://dhsnext.console.aliyun.com/cn-hangzhou/projects)