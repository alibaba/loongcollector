# 输入插件

输入插件是 LoongCollector 的核心组件之一，负责从各类数据源高效采集数据。LoongCollector 提供两种类型的输入插件，分别针对不同的使用场景:

- 原生插件(C++): 高性能、低开销的首选方案
- 扩展插件(Golang): 灵活可扩展的补充方案

## 插件类型介绍

### 原生插件

原生插件采用 C++实现，具有以下显著优势:

- 卓越的性能表现和极低的资源开销
- 专注于常见数据源的高效采集
- 生产环境首选的稳定采集方案

| 名称                                                                           | 提供方   | 功能简介                              |
| ------------------------------------------------------------------------------ | -------- | ------------------------------------- |
| `input_file`<br>[文本日志](native/input-file.md)                               | SLS 官方 | 文本采集。                            |
| `input_container_stdio`<br> [容器标准输出](native/input-container-stdio.md)    | SLS 官方 | 从容器标准输出/标准错误流中采集日志。 |
| `input_file_security`<br>[文件安全数据](native/input-file-security.md)         | SLS 官方 | 文件安全数据采集。                    |
| `input_network_observer`<br>[网络可观测数据](native/input-network-observer.md) | SLS 官方 | 网络可观测数据采集。                  |
| `input_network_security`<br>[网络安全数据](native/input-network-security.md)   | SLS 官方 | 网络安全数据采集。                    |
| `input_process_security`<br>[进程安全数据](native/input-process-security.md)   | SLS 官方 | 进程安全数据采集。                    |
| `input_internal_metrics`<br>[自监控指标数据](native/input-internal-metrics.md) | SLS 官方 | 导出自监控指标数据。                  |
| `input_internal_alarms`<br>[自监控告警数据](native/input-internal-alarms.md)   | SLS 官方 | 导出自监控告警数据。                  |

### 扩展插件

扩展插件基于 Golang 实现，具有以下特点:

- 性能与资源开销均衡
- 支持丰富多样的数据源接入
- 开发门槛低，易于定制与扩展
- 适用于特定场景的数据采集需求

| 名称                                                                                  | 提供方                                                | 功能简介                                                                         |
| ------------------------------------------------------------------------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------------- |
| `input_command`<br>[脚本执行数据](extended/input-command.md)                          | 社区<br>[didachuxing](https://github.com/didachuxing) | 采集脚本执行数据。                                                               |
| `input_docker_stdout`<br>[容器标准输出](extended/service-docker-stdout.md)            | SLS 官方                                              | 从容器标准输出/标准错误流中采集日志。                                            |
| `metric_debug_file`<br>[文本日志（debug）](extended/metric-debug-file.md)             | SLS 官方                                              | 用于调试的读取文件内容的插件。                                                   |
| `metric_input_example`<br>[MetricInput 示例插件](extended/metric-input-example.md)    | SLS 官方                                              | MetricInput 示例插件。                                                           |
| `metric_meta_host`<br>[主机 Meta 数据](extended/metric-meta-host.md)                  | SLS 官方                                              | 主机 Meta 数据。                                                                 |
| `metric_mock`<br>[Mock 数据-Metric](extended/metric-mock.md)                          | SLS 官方                                              | 生成 metric 模拟数据的插件。                                                     |
| `metric_system_v2`<br>[主机监控数据](extended/metric-system.md)                       | SLS 官方                                              | 主机监控数据。                                                                   |
| `service_canal`<br>[MySQL Binlog](extended/service-canal.md)                          | SLS 官方                                              | 将 MySQL Binlog 输入到 iLogtail。                                                |
| `service_go_profile`<br>[GO Profile](extended/service-goprofile.md)                   | SLS 官方                                              | 采集 Golang pprof 性能数据。                                                     |
| `service_gpu_metric`<br>[GPU 数据](extended/service-gpu.md)                           | SLS 官方                                              | 支持收集英伟达 GPU 指标。                                                        |
| `service_http_server`<br>[HTTP 数据](extended/service-http-server.md)                 | SLS 官方                                              | 接收来自 unix socket、http/https、tcp 的请求，并支持 sls 协议、otlp 等多种协议。 |
| `service_input_example`<br>[ServiceInput 示例插件](extended/service-input-example.md) | SLS 官方                                              | ServiceInput 示例插件。                                                          |
| `service_journal`<br>[Journal 数据](extended/service-journal.md)                      | SLS 官方                                              | 从原始的二进制文件中采集 Linux 系统的 Journal（systemd）日志。                   |
| `service_kafka`<br>[Kafka](extended/service-kafka.md)                                 | SLS 官方                                              | 将 Kafka 数据输入到 iLogtail。                                                   |
| `service_mock`<br>[Mock 数据-Service](extended/service-mock.md)                       | SLS 官方                                              | 生成 service 模拟数据的插件。                                                    |
| `service_mssql`<br>[SqlServer 查询数据](extended/service-mssql.md)                    | SLS 官方                                              | 将 Sql Server 数据输入到 iLogtail。                                              |
| `service_otlp`<br>[OTLP 数据](extended/service-otlp.md)                               | 社区<br>[Zhu Shunjia](https://github.com/shunjiazhu)  | 通过 http/grpc 协议，接收 OTLP 数据。                                            |
| `service_pgsql`<br>[PostgreSQL 查询数据](extended/service-pgsql.md)                   | SLS 官方                                              | 将 PostgresSQL 数据输入到 iLogtail。                                             |
| `service_snmp`<br>[收集 SNMP 协议机器信息](extended/service-snmp.md)                  | SLS 官方                                              | 收集 SNMP 协议机器信息.                                                          |
| `service_syslog`<br>[Syslog 数据](extended/service-syslog.md)                         | SLS 官方                                              | 采集 syslog 数据。                                                               |

## 插件特性对比

| 特性     | 原生插件     | 扩展插件 |
| -------- | ------------ | -------- |
| 实现语言 | C++          | Golang   |
| 性能表现 | 极致性能     | 性能适中 |
| 资源开销 | 极低开销     | 开销适中 |
| 功能覆盖 | 专注常见场景 | 广泛覆盖 |
| 开发难度 | 中等         | 较低     |

## 选型建议

1. 推荐使用原生插件的场景:

   - 对性能和资源消耗有严格要求
   - 采集常见标准数据源
   - 部署在资源受限环境

2. 适合使用扩展插件的场景:
   - 需要采集特殊或自定义数据源
   - 有特定的定制化需求
   - 需要快速开发和迭代
   - 性能要求相对灵活

## 使用说明

- 插件组合规则:
  - 原生 Input 插件: 可配合原生/扩展 Processor 插件使用，支持 SPL 插件
  - 扩展 Input 插件: 仅支持扩展 Processor 插件
  - 详细说明请参考[处理插件文档](../processor/processors.md)
