# DefaultEncoder Encoder扩展

## 简介

[ext_default_encoder](https://github.com/alibaba/ilogtail/blob/main/plugins/extension/default_encoder/default_encoder.go)
扩展，实现了 [Encoder](https://github.com/alibaba/ilogtail/blob/main/pkg/pipeline/extensions/encoder.go) 接口，可以用在
`flusher_http` 等插件中用于序列化不同的协议数据。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数          | 类型     | 是否必选 | 说明                                                                                                        |
|-------------|--------|------|-----------------------------------------------------------------------------------------------------------|
| Format      | String | 是    | 具体的协议，[查看支持的具体协议列表](https://github.com/alibaba/ilogtail/blob/master/pkg/protocol/encoder/common/comon.go) |
| SeriesLimit | Int    | 否    | 触发序列化时序切片的最大长度，默认 1000，仅针对 Format=prometheus 时有效                                                          |

## 样例

使用 `flusher_http` flusher 插件，配置发送 `prometheus` 协议数据。

```yaml
enable: true
flushers:
- Type: flusher_http
  ...
  Encoder:
    Type: ext_default_encoder/prometheus
    ...
...
extensions:
- Type: ext_default_encoder/prometheus
  Format: 'prometheus'
  SeriesLimit: 1024
```
