# AWS S3

## 简介

`flusher_s3` `flusher`插件可以实现将采集到的数据，经过处理后，发送到 AWS S3。

## 版本

[Alpha](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                 |
|-----------------------------------|----------|------|------------------------------------------------------------------------------------|
| Region                            | String   | 是    | S3 bucket 的地域 |
| Bucket                            | String   | 是    | S3 bucket 的名字 |
| Profile                           | String   | 否    | 指定 AWS 配置文件以提供凭据的选项，用于指定权限，默认为`default`。如何配置ak、sk可以参考[文档](https://docs.aws.amazon.com/sdkref/latest/guide/file-format.html)。 |
| S3KeyFormat                       | String   | 否    | S3 键的格式字符串。此选项支持`${UUID}`、strftime 时间格式器。在格式字符串中添加 `${UUID}` 以插入一个随机字符串。默认值： `/loongcollector-logs/%Y-%m-%d/%H-%M-%S-${UUID}.log` |
| TotalFileSize                     | Int      | 否    | S3 中写入的文件大小，单位为byte。最小大小： 1024（1K） ，最大大小： 1073741824（1G）。默认值： 104857600（100M）。 |
| UploadTimeout                     | Int      | 否    | 超时时间，单位为秒。超过该时间，LoongCollector 将在 S3 中上传并创建一个新的文件。设置为 3600 表示每小时上传一个新文件。最小值：10，默认值：3600。 |
| Convert                           | Struct   | 否    | ilogtail数据转换协议配置                                                                   |
| Convert.Protocol                  | String   | 否    | ilogtail数据转换协议，kafka flusher 可选值：`custom_single`,`otlp_log_v1`。默认值：`custom_single` |
| Convert.Encoding                  | String   | 否    | ilogtail flusher数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                     |
| Convert.TagFieldsRename           | Map      | 否    | 对日志中tags中的json字段重命名                                                                |
| Convert.ProtocolFieldsRename      | Map      | 否    | ilogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                              |

## 说明

LoongCollector 会将采集到的数据保存在内存buffer中，有序写入 S3 Object。Object 的 Key 通过 `S3KeyFormat` 指定，每个 Object 的大小由 `TotalFileSize` 决定。

Object 的轮转由几个因素触发：

* Object 大小达到`TotalFileSize`。
* 上一个 Object 的创建时间超过了`UploadTimeout`。
* `flusher_s3`所属的采集配置更新/移除，或 LoongCollector 停止，会立刻将现有 buffer 上传。

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 S3，每个文件的大小设置为100K。这里我们假设待采集的文件的数据写入量远大于100K/hour。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_s3
    Region: us-east-1
    Bucket: flusher-s3-test
    TotalFileSize: 102400
```

会在 S3 的 bucket flusher-s3-test中，看到多个大小约为 100K，格式类似`loongcollector-logs/2025-03013/09-26-26-GOCAvAwq.log`的 Object，里面正是采集上来的数据。
