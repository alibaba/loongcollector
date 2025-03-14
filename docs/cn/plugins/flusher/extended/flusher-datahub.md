# Datahub

## 简介

`flusher_datahub` `flusher`插件可以实现将采集到的数据，经过处理后，发送到阿里云数据总线[Datahub](https://www.aliyun.com/product/bigdata/datahub)。

## 版本

[Alpha](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                 |
|-------------------------------------|----------|------|------------------------------------------------------------------------------------|
| Type                                | String    | 是    | 插件类型，固定为`flusher_datahub`
| Endpoint                            | String    | 是    | 数据总线 Datahub 的域名 |
| AccessKeyID                         | String    | 是    | 阿里云账号下的 AccessKeyID |
| AccessKeySecret                     | String    | 是    | 阿里云账号下的 AccessKeySecret，与上述 AccessKeyID 配合使用 |
| SecurityToken                       | String    | 否    | 阿里云账号下的 SecurityToken，sts模式访问下的临时令牌，可参考[访问控制](https://help.aliyun.com/zh/ram/product-overview/what-is-sts?spm=5176.21213303.J_v8LsmxMG6alneH-O7TCPa.13.33f62f3dxLRJOL&scm=20140722.S_help@@%E6%96%87%E6%A1%A3@@28756._.ID_help@@%E6%96%87%E6%A1%A3@@28756-RL_ststoken-LOC_2024SPHelpResult-OR_ser-PAR1_2150422117422070235704062e9fb7-V_4-RE_new3-P0_3-P1_0) |
| ProjectName                         | String    | 是    | 数据总线 Datahub 项目名称，可在控制台中创建和查询 |
| TopicName                           | String    | 是    | 数据总线 Datahub 项目名称，可在控制台中创建和查询 |

## 说明

LoongCollector 会将采集到的数据会直接通过 PutRecord 接口写入到 Datahub 中。

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 Datahub。

```yaml
enable: true
inputs:
    - FilePaths:
        - /home/test-log/*.log
      Type: input_file
flushers:
    - Type: flusher_datahub
      AccessKeyID: xxxxx
      AccessKeySecret: xxxxx
      Endpoint: http://dh-cn-hangzhou.aliyun-inc.com
      ProjectName: project_sample
      TopicName: topic_sample
```
可以在阿里云数据总线Datahub控制台上看到已采集到的数据。[Datahub控制台](https://dhsnext.console.aliyun.com/cn-hangzhou/projects)