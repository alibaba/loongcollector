# 转发输入插件

## 简介

`input_forward` 插件用于接收来自其他系统的数据转发请求，目前支持LoongSuite协议。该插件可以作为数据转发的接收端，通过配置的匹配规则来处理接收到的数据。[源代码](https://github.com/alibaba/loongcollector/blob/main/core/plugin/input/InputForward.h)

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v2.0.0 及以上

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`input_forward`。 |
| Protocol | String，无默认值（必填） | 转发协议类型。目前支持：`LoongSuite`。 |
| Endpoint | String，无默认值（必填） | 监听地址和端口，格式为`IP:PORT`，例如`0.0.0.0:7899`。或者本地通信socket，例如`/root/loongcollector.sock`。 |
| MatchRule | Object，无默认值（必填） | 匹配规则配置对象。 |
| MatchRule.Value | String，无默认值（必填） | 用于匹配请求的标识值，通过请求的metadata中指定字段进行匹配。不同协议规定使用不同字段。`LoongSuite`: `X-Loongsuite-Apm-Configname`。 |

## 样例

### 接收LoongSuite协议数据

* 采集配置

```yaml
enabled: true
inputs:
  - Type: input_forward
    Protocol: LoongSuite
    Endpoint: 0.0.0.0:7899
    MatchRule:
      Value: test-config
flushers:
  - Type: flusher_sls
    Project: "your-project"
    Logstore: "your-logstore"
    Region: cn-shanghai
    Endpoint: cn-shanghai.log.aliyuncs.com
```

* 输入

通过gRPC客户端发送LoongSuite协议的转发请求，请求需要在metadata中包含`x-loongsuite-apm-configname: test-config`字段，数据内容在请求的data字段中，为 `PipelineEventGroup` PB 序列化的结果。

* 输出

```json
{
    "content": "接收到的原始数据内容",
    "__time__": "1642502400"
}
```
