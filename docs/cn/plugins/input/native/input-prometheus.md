# Prometheus 抓取（原生插件）

## 简介

`input_prometheus` 插件按 `ScrapeConfig` 配置抓取 Prometheus 指标并输出到 `raw_event`中。默认使用 `ProcessorPromParseMetricNative` 和 `ProcessorPromRelabelMetricNative` 两个 `Processor` 进行指标解析和处理。

## 版本

[Beta](../../stability-level.md)

## 配置参数

### 插件级参数

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| Type | string | 是 | / | 固定为 `input_prometheus` |
| ScrapeConfig | object | 是 | / | 兼容 Prometheus 采集配置对象，详见下表。 |

### ScrapeConfig 参数

ScrapeConfig 兼容绝大部分 [Prometheus scrape_config 采集配置](https://prometheus.io/docs/prometheus/latest/configuration/configuration/) ，下表是已兼容的配置项目。

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| job_name | string | 是 | / | 作业名称，用于标识该采集任务。 |
| scrape_interval | string | 否 | 60s | 抓取间隔，支持格式如 `15s`、`1m` 等。 |
| scrape_timeout | string | 否 | 10s | 抓取超时时间，必须小于 `scrape_interval`。 |
| metrics_path | string | 否 | /metrics | 指标端点路径。 |
| honor_labels| bool | 否 | false | 是否保留目标暴露的标签，当与采集器标签冲突时。 |
| honor_timestamps| bool | 否 | true | 是否保留指标中的时间戳。 |
| scheme | string | 否 | http | 抓取协议，可选 `http` 或 `https`。 |
| params | object | 否 | / | URL 查询参数 |
| static_configs | array | 否 | / | 静态目标配置列表，详见下表。 |
| relabel_configs | array | 否 | / | 目标重标签配置列表，用于在抓取前修改目标标签。 |
| metric_relabel_configs | array | 否 | / | 指标重标签配置列表，用于在抓取后修改指标标签。 |
| scrape_protocols | array | 否 | / | 支持的抓取协议列表，可选值：`PrometheusText0.0.4`、`PrometheusProto`、`OpenMetricsText0.0.1`、`OpenMetricsText1.0.0`。默认包含所有协议。 |
| external_labels | object | 否 | / | 外部标签，会添加到所有抓取的指标中，格式为键值对。 |
| host_only_mode | bool | 否 | false | 是否启用主机模式，启用后会禁用向 Operator 的服务发现。 |
| basic_auth | object | 否 | / | BasicAuth 授权配置，详见下表。|
| authorization | object | 否 | / | HTTP 授权配置，详见下表。与 `basic_auth` 不能同时使用。 |
| follow_redirects | bool | 否 | true | 是否跟随 HTTP 重定向。 |
| tls_config | object | 否 | / | TLS 配置对象，详见下表。 |

### static_configs 参数

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| targets | array | 是 | / | 目标地址列表，格式为 `["host1:port1", "host2:port2"]`。 |
| labels | object | 否 | / | 附加到该配置下所有目标的标签，格式为键值对。 |

### basic_auth 参数

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| username | string | 条件必填 | / | 用户名。`username` 和 `username_file` 二选一。 |
| username_file | string | 条件必填 | / | 存储用户名的文件路径。`username` 和 `username_file` 二选一。 |
| password | string | 条件必填 | / | 密码。`password` 和 `password_file` 二选一。 |
| password_file | string | 条件必填 | / | 存储密码的文件路径。`password` 和 `password_file` 二选一。 |

### authorization 参数

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| type | string | 否 | Bearer | 授权类型，如 `Bearer`。 |
| credentials | string | 条件必填 | / | 授权凭证。`credentials` 和 `credentials_file` 二选一。 |
| credentials_file | string | 条件必填 | / | 存储授权凭证的文件路径。`credentials` 和 `credentials_file` 二选一。 |

### tls_config 参数

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| ca_file | string | 否 | / | CA 证书文件路径。 |
| cert_file | string | 否 | / | 客户端证书文件路径。 |
| key_file | string | 否 | / | 客户端私钥文件路径。 |
| server_name | string | 否 | / | TLS 服务器名称，用于 SNI。 |
| insecure_skip_verify | bool | 否 | false | 是否跳过 TLS 证书验证（不推荐在生产环境使用）。 |

## 注意事项

1. **必填项**：`job_name` 为必填项，其他配置项可根据需要选填。

2. **本地采集模式**：`host_only_mode` 默认为 `false`，会以 Operator 模式运行，通过 Operator 服务发现，接受分配的采集目标。禁用 Operator 模式需要设置为 `true`，且填写 `static_configs`。

3. **本地采集模式可用 labels 列表**：

    1. __host_hostname：当前主机名称
    2. __host_ip：当前主机网络 IP
    3. __ecs_meta_instance_id：ECS 实例 ID（仅在阿里云 ECS 环境中可用）
    4. __ecs_meta_region_id：ECS 地域 ID（仅在阿里云 ECS 环境中可用）
    5. __ecs_meta_zone_id：ECS 可用区 ID（仅在阿里云 ECS 环境中可用）
    6. __ecs_meta_user_id：ECS 用户 ID，即账号 ID（仅在阿里云 ECS 环境中可用）
    7. __ecs_meta_vpc_id：ECS VPC ID（仅在阿里云 ECS 环境中可用）
    8. __ecs_meta_vswitch_id：ECS 虚拟交换机 ID（仅在阿里云 ECS 环境中可用）

## 样例

### 基础配置

采集本地 Node Exporter 的指标。

```yaml
enable: true
inputs:
  - Type: input_prometheus
    ScrapeConfig:
      job_name: node
      host_only_mode: true
      scrape_interval: 15s
      scrape_timeout: 10s
      static_configs:
        - targets: ["127.0.0.1:9100"]
          labels:
            env: production
            region: cn-beijing
flushers:
  - Type: flusher_sls
    Aliuid": "your uid"
    Endpoint": "cn-beijing.log.aliyuncs.com"
    Project": "your project"
    Logstore": "your logstore"
    Region": "cn-beijing"
    TelemetryType": "metrics"
```
