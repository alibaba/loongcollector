# DefaultDecoder Decoder扩展

## 简介

[ext_default_decoder](https://github.com/alibaba/loongcollector/blob/main/plugins/extension/default_decoder/default_decoder.go) 扩展，实现了 [Decoder](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/extensions/decoder.go) 接口，可以用在 `service_http_server`、`udp_server` 等插件中用于解析不同的协议数据。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数                | 类型      | 是否必选 | 说明                                                                                                  |
|-------------------|---------|------|-----------------------------------------------------------------------------------------------------|
| Format            | String  | 是    | 具体的协议，[查看支持的具体协议列表](https://github.com/alibaba/loongcollector/blob/master/pkg/protocol/decoder/common/comon.go) |
| FieldsExtend      | Boolean | 否    | 是否启用增强字段功能，默认为false，仅针对Format=influxdb时有效                                                           |
| DisableUncompress | Boolean | 是    | 否不解压数据，默认为false，仅针对Format=raw时有效                                                                    |

## 样例

使用 `service_http_server` input 插件，配置接收 `influxdb` 协议数据。

```yaml
enable: true
inputs:
  - Type: service_http_server
    Address: ":18888"
    Format: influxdb
    Decoder: ext_default_decoder
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

使用如下命令测试：

```shell
curl http://localhost:18888/write?db=mydb --data-binary 'weather,city=hz value=34'
```
