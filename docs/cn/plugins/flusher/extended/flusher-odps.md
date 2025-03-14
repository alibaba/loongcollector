# Odps

## 简介

`flusher_odps` `flusher`插件可以实现将采集到的数据，经过处理后，发送到阿里云大数据计算服务[Maxcompute(又名Odps)](https://maxcompute.console.aliyun.com/cn-hangzhou/project-list)。

## 版本

[Deprecated](../../stability-level.md)

## 版本说明

* 推荐版本：无

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                 |
|-------------------------------------|----------|------|------------------------------------------------------------------------------------|
| Type                                | String    | 是    | 插件类型，固定为`flusher_odps`
| Endpoint                            | String    | 是    | 待访问 Odps 的域名 |
| AccessKeyId                         | String    | 是    | 阿里云账号下的 AccessKeyId |
| AccessKeySecret                     | String    | 是    | 阿里云账号下的 AccessKeySecret，与上述 AccessKeyId 配合使用 |
| ProjectName                         | String    | 是    | Odps 项目名称，可在控制台中创建和查询 |
| TableName                           | String    | 是    | Odps 目标表名称，可在控制台中创建和查询 |
| PartitionConfig                     | String    | 否    | Odps 目标表的分区模式，仅分区表中使用 |
| TimeRange                           | String    | 否    | Odps 目标表的分区大小，仅分区表中使用 |


## 说明

LoongCollector 会将采集到的数据会直接通过 Append 接口写入到 Odps 中。

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 Odps，每个文件的大小设置为100K。这里我们假设待采集的文件的数据写入量远大于100K/hour。

```yaml
enable: true
inputs:
    - FilePaths:
        - /home/test-log/*.log
      Type: input_file
flushers:
    - Type: flusher_odps
      AccessKeyId: xxxxx
      AccessKeySecret: xxxxx
      Endpoint: https://service.cn-hangzhou-vpc.maxcompute.aliyun-inc.com/api
      ProjectName: project_sample
      TableName: table_sample
      PartitionConfig:  ds={{%Y%m%d}},hh={{%H}},mm={{%M}}
```
可以在阿里云大数据计算服务控制台上查询到已采集到的数据。[Maxcompute](https://maxcompute.console.aliyun.com/cn-hangzhou/project-list)
