# 网络连通性监控

## 简介

`metric_input_netping` 是一个网络连通性监控插件，可以从本机发起 ICMP Ping、TCPing 和 HTTPing，并将探测结果作为 Metric 事件输出。插件还可以记录目标主机的 DNS 解析耗时。

插件源码：[plugins/input/netping/netping.go](https://github.com/alibaba/loongcollector/blob/main/plugins/input/netping/netping.go)

插件按采集节点处理配置：只有 `src` 与当前节点 IP 匹配的 ICMP、TCP 或 HTTP 配置才会在该节点执行。开启 `local_trigger_mode` 后，`src` 为空的配置会使用当前节点 IP。

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| — | ✓ | — |

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.4.1 及以上

## 配置参数

| 参数 | 类型，默认值 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | String，无默认值（必填） | 是 | 插件类型，固定为 `metric_input_netping`。 |
| disable_dns_metric | Boolean，`false` | 否 | 是否关闭 DNS 解析耗时和成功/失败指标。关闭后仍执行 ICMP、TCP 或 HTTP 探测。 |
| timeout_seconds | Integer，`5` | 否 | ICMP、TCP 探测的超时时间，单位为秒。取值小于等于 1 或大于 30 时回退为 5。 |
| interval_seconds | Integer，`60` | 否 | 探测间隔，单位为秒。取值小于等于 5 或大于 86400 时回退为 60；如果超时时间大于间隔，超时时间回退为 5。 |
| icmp | Array，`[]` | 否 | ICMP 探测配置列表。每项包含 `src`、`target`、`count`，可选 `name` 和 `labels`。 |
| tcp | Array，`[]` | 否 | TCP 探测配置列表。每项包含 `src`、`target`、`port`、`count`，可选 `name` 和 `labels`。 |
| http | Array，`[]` | 否 | HTTP 探测配置列表。每项包含 `src`、`target`，可选 `method`、`expect_response_contains`、`expect_code`、`name` 和 `labels`。 |
| local_trigger_mode | Boolean，`false` | 否 | 当配置项的 `src` 为空时，是否使用当前节点 IP 作为探测源。 |

### ICMP 配置项

| 参数 | 类型，默认值 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| src | String，`""` | 是 | 探测源 IP。只有与当前节点 IP 相同的配置项会在该节点执行；开启 `local_trigger_mode` 后可以留空。 |
| target | String，无默认值（必填） | 是 | 目标 IP 或域名。 |
| count | Integer，无默认值（必填） | 是 | 每次采集执行的 ICMP 请求次数。 |
| name | String，`""` | 否 | 指标的 `name` 标签。为空时自动使用 `src -> target`。 |
| labels | String 字典，`{}` | 否 | 添加到指标上的自定义标签。 |

### TCP 配置项

| 参数 | 类型，默认值 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| src | String，`""` | 是 | 探测源 IP。只有与当前节点 IP 相同的配置项会在该节点执行；开启 `local_trigger_mode` 后可以留空。 |
| target | String，无默认值（必填） | 是 | 目标 IP 或域名。 |
| port | Integer，无默认值（必填） | 是 | TCP 目标端口。 |
| count | Integer，无默认值（必填） | 是 | 每次采集执行的 TCP 连接次数。 |
| name | String，`""` | 否 | 指标的 `name` 标签。为空时自动使用 `src -> target:port`。 |
| labels | String 字典，`{}` | 否 | 添加到指标上的自定义标签。 |

### HTTP 配置项

| 参数 | 类型，默认值 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| src | String，`""` | 是 | 探测源 IP。只有与当前节点 IP 相同的配置项会在该节点执行；开启 `local_trigger_mode` 后可以留空。 |
| target | String，无默认值（必填） | 是 | HTTP 或 HTTPS URL。未填写协议时默认使用 `http://`。 |
| method | String，`GET` | 否 | HTTP 请求方法。 |
| expect_response_contains | String，`""` | 否 | 期望响应正文包含的字符串。配置后，正文不包含该字符串时本次探测失败。 |
| expect_code | Integer，`200` | 否 | 期望的 HTTP 状态码。实际状态码不匹配时本次探测失败。 |
| name | String，`""` | 否 | 指标的 `name` 标签。为空时自动使用 `src -> scheme://host`。 |
| labels | String 字典，`{}` | 否 | 添加到指标上的自定义标签。 |

## 输出指标

### ICMP 和 TCP

插件会为每个探测配置输出以下指标，其中 `ping` 表示 ICMP，`tcping` 表示 TCP：

- `<type>_total`：本次探测总次数。
- `<type>_success`：成功次数。
- `<type>_failed`：失败次数。
- `<type>_rtt_min_ms`：成功请求的最小 RTT，单位为毫秒。
- `<type>_rtt_max_ms`：成功请求的最大 RTT，单位为毫秒。
- `<type>_rtt_avg_ms`：成功请求的平均 RTT，单位为毫秒。
- `<type>_rtt_total_ms`：成功请求 RTT 总和，单位为毫秒。
- `<type>_rtt_stddev_ms`：成功请求 RTT 的标准差，单位为毫秒。

ICMP 指标通常包含 `name`、`src`、`dst`、`src_host` 标签；TCP 指标还包含 `port` 标签。`labels` 中的自定义标签也会写入指标。

### HTTP

HTTP 探测会输出：

- `httping_total`、`httping_success`、`httping_failed`。
- `httping_rt_ms`：HTTP 请求耗时，单位为毫秒；仅在请求成功时输出。
- `httping_response_bytes`：响应正文大小；仅在请求成功时输出。
- `httping_cert_ttl_days`：HTTPS 证书剩余天数；仅在响应包含可识别证书时输出。

HTTP 指标包含 `name`、`src`、`url`、`src_host`、`method` 等标签，并会附带响应协议、URL、状态码等标签。

### DNS

未设置 `disable_dns_metric: true` 时，插件会对探测目标执行 DNS 解析并输出：

- `dns_resolve_rt_ms`：DNS 解析耗时，单位为毫秒；解析失败时不输出该值。
- `dns_resolve_success`：解析成功为 1，否则为 0。
- `dns_resolve_failed`：解析失败为 1，否则为 0。

DNS 指标包含 `dns_name`、`src` 和 `src_host` 标签。配置为 IP 地址的目标不会产生 DNS 解析指标。

## 配置示例

下面的配置从本机执行 ICMP、TCP 和 HTTPS 探测，并把结果输出到标准输出：

```yaml
enable: true
inputs:
  - Type: metric_input_netping
    interval_seconds: 60
    timeout_seconds: 5
    icmp:
      - src: "192.0.2.10"
        target: "192.0.2.20"
        count: 3
        name: "gateway-icmp"
        labels:
          environment: "production"
    tcp:
      - src: "192.0.2.10"
        target: "example.com"
        port: 443
        count: 3
        name: "example-https"
    http:
      - src: "192.0.2.10"
        target: "https://example.com/health"
        method: "GET"
        expect_code: 200
        expect_response_contains: "ok"
    local_trigger_mode: false
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

如果同一配置会部署到不同节点，可以让插件自动使用本机 IP 作为探测源：

```yaml
inputs:
  - Type: metric_input_netping
    local_trigger_mode: true
    icmp:
      - target: "192.0.2.20"
        count: 3
```

## 输出示例

每条 Metric 事件输出为一个独立 JSON 对象。下面是一次 ICMP 探测的示意：

```json
{
  "__name__": "ping_rtt_avg_ms",
  "__labels__": "name#$#gateway-icmp|src#$#192.0.2.10|dst#$#192.0.2.20|src_host#$#collector-1|environment#$#production",
  "__value__": "1.234",
  "__time__": "1700000000"
}
```