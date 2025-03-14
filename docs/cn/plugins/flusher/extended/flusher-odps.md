# Odps

## 简介

`flusher_odps` `flusher`插件可以实现将采集到的数据，经过处理后，发送到阿里云大数据计算服务[Maxcompute(又名Odps)](https://www.aliyun.com/product/maxcompute)。

## 版本

[Alpha](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                 |
|-------------------------------------|----------|------|------------------------------------------------------------------------------------|
| Type                                | String    | 是    | 插件类型，固定为`flusher_odps`
| Endpoint                            | String    | 是    | 待访问 Odps 的域名 |
| AccessKeyID                         | String    | 是    | 阿里云账号下的 AccessKeyID |
| AccessKeySecret                     | String    | 是    | 阿里云账号下的 AccessKeySecret，与上述 AccessKeyID 配合使用 |
| SecurityToken                       | String    | 否    | 阿里云账号下的 SecurityToken，sts模式访问下的临时令牌，可参考[访问控制](https://help.aliyun.com/zh/ram/product-overview/what-is-sts?spm=5176.21213303.J_v8LsmxMG6alneH-O7TCPa.13.33f62f3dxLRJOL&scm=20140722.S_help@@%E6%96%87%E6%A1%A3@@28756._.ID_help@@%E6%96%87%E6%A1%A3@@28756-RL_ststoken-LOC_2024SPHelpResult-OR_ser-PAR1_2150422117422070235704062e9fb7-V_4-RE_new3-P0_3-P1_0)。如果使用AK模式则不填写该字段 |
| ProjectName                         | String    | 是    | Odps 项目名称，可在控制台中创建和查询 |
| SchemaName                          | String    | 否    | Odps 中介于表和分区之间的概念，可参考[Schema](https://help.aliyun.com/zh/maxcompute/user-guide/schemas?scm=20140722.S_help%40%40%E6%96%87%E6%A1%A3%40%40611216.S_BB2%40bl%2BRQW%40ag0%2BBB1%40ag0%2Bhot%2Bos0.ID_611216-RL_schema-LOC_doc%7EUND%7Eab-OR_ser-PAR1_2102029c17422074764254704de7a9-V_4-P0_0-P1_0&spm=a2c4g.11174283.help-search.i39) |
| TableName                           | String    | 是    | Odps 目标表名称，可在控制台中创建和查询 |
| PartitionConfig                     | String    | 否    | Odps 目标表的分区模式，仅分区表中使用,非分区模式下不需要填写。示例: ds={{%Y%m%d}},hh={{%H}},mm={{%M}} |
| TimeRange                           | String    | 否    | Odps 目标表的分区大小，仅分区表中使用,非分区模式下不需要填写, 单位为分钟。示例: 15 |


## 说明

LoongCollector 会将采集到的数据会直接通过 Append 接口写入到 Odps 中。

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 Odps。

```yaml
enable: true
inputs:
    - FilePaths:
        - /home/test-log/*.log
      Type: input_file
flushers:
    - Type: flusher_odps
      AccessKeyID: xxxxx
      AccessKeySecret: xxxxx
      Endpoint: https://service.cn-hangzhou-vpc.maxcompute.aliyun-inc.com/api
      ProjectName: project_sample
      TableName: table_sample
      TimeRange: 15
      PartitionConfig:  ds={{%Y%m%d}},hh={{%H}},mm={{%M}}
```
可以在阿里云大数据计算服务控制台上查询到已采集到的数据。[Maxcompute](https://maxcompute.console.aliyun.com/cn-hangzhou/project-list)
