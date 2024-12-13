# 输入插件

输入插件是LoongCollector的核心组件之一，负责从各类数据源高效采集数据。LoongCollector提供两种类型的输入插件，分别针对不同的使用场景:

- 原生插件(C++): 高性能、低开销的首选方案
- 扩展插件(Golang): 灵活可扩展的补充方案

## 插件类型介绍

### 原生插件

原生插件采用C++实现，具有以下显著优势:

- 卓越的性能表现和极低的资源开销
- 专注于常见数据源的高效采集
- 生产环境首选的稳定采集方案

| 名称 | 提供方 | 功能简介 |
|------|--------|----------|
| [`input_file`](native/input-file.md)<br> 文本日志                                      | SLS官方 | 文本采集。                                                 |
| [`input_container_stdio`](native/input-container-stdio.md)<br> 容器标准输出（原生插件）                                      | SLS官方 | 从容器标准输出/标准错误流中采集日志。                                                 |
| [`input_observer_network`](native/metric-observer.md)<br>eBPF网络调用数据         | SLS官方                                                      | 支持从网络系统调用中收集四层网络调用，并借助网络解析模块，可以观测七层网络调用细节。 |
| [`input_file_security`](native/input-file-security.md)<br> 文件安全数据                                      | SLS官方 | 文件安全数据采集。                                                 |
| [`input_network_observer`](native/input-network-observer.md)<br> 网络可观测数据                                      | SLS官方 | 网络可观测数据采集。                                                 |
| [`input_network_security`](native/input-network-security.md)<br> 网络安全数据                                      | SLS官方 | 网络安全数据采集。                                                 |
| [`input_process_security`](native/input-process-security.md)<br> 进程安全数据                                      | SLS官方 | 进程安全数据采集。                                                 |

### 扩展插件

扩展插件基于Golang实现，具有以下特点:

- 性能与资源开销均衡
- 支持丰富多样的数据源接入
- 开发门槛低，易于定制与扩展
- 适用于特定场景的数据采集需求

| 名称 | 提供方 | 功能简介 |
|------|--------|----------|
| [`input_command`](extended/input-command.md)<br>脚本执行数据                           | 社区<br>[`didachuxing`](https://github.com/didachuxing)      | 采集脚本执行数据。                                             |
| [`input_docker_stdout`](extended/service-docker-stdout.md)<br>容器标准输出             | SLS官方                                                      | 从容器标准输出/标准错误流中采集日志。                                   |
| [`metric_debug_file`](extended/metric-debug-file.md)<br>文本日志（debug）              | SLS官方                                                      | 用于调试的读取文件内容的插件。                                       |
| [`metric_input_example`](extended/metric-input-example.md)<br>MetricInput示例插件    | SLS官方                                                      | MetricInput示例插件。                                      |
| [`metric_meta_host`](extended/metric-meta-host.md)<br>主机Meta数据                   | SLS官方                                                      | 主机Meta数据。                                             |
| [`metric_mock`](extended/metric-mock.md)<br>Mock数据-Metric                        | SLS官方                                                      | 生成metric模拟数据的插件。                                      |
| [`metric_system_v2`](extended/metric-system.md)<br>主机监控数据                        | SLS官方                                                      | 主机监控数据。                                               |
| [`service_canal`](extended/service-canal.md)<br>MySQL Binlog                     | SLS官方                                                      | 将MySQL Binlog输入到iLogtail。                             |
| [`service_go_profile`](extended/service-goprofile.md)<br>GO Profile              | SLS官方                                                      | 采集Golang pprof 性能数据。                                  |
| [`service_gpu_metric`](extended/service-gpu.md)<br>GPU数据                         | SLS官方                                                      | 支持收集英伟达GPU指标。                                         |
| [`service_http_server`](extended/service-http-server.md)<br>HTTP数据               | SLS官方                                                      | 接收来自unix socket、http/https、tcp的请求，并支持sls协议、otlp等多种协议。 |
| [`service_input_example`](extended/service-input-example.md)<br>ServiceInput示例插件 | SLS官方                                                      | ServiceInput示例插件。                                     |
| [`service_journal`](extended/service-journal.md)<br>Journal数据                    | SLS官方                                                      | 从原始的二进制文件中采集Linux系统的Journal（systemd）日志。               |
| [`service_kafka`](extended/service-kafka.md)<br>Kafka                            | SLS官方                                                      | 将Kafka数据输入到iLogtail。                                  |
| [`service_mock`](extended/service-mock.md)<br>Mock数据-Service                     | SLS官方                                                      | 生成service模拟数据的插件。                                     |
| [`service_mssql`](extended/service-mssql.md)<br>SqlServer查询数据                    | SLS官方                                                      | 将Sql Server数据输入到iLogtail。                             |
| [`service_otlp`](extended/service-otlp.md)<br>OTLP数据                             | 社区<br>[`Zhu Shunjia`](https://github.com/shunjiazhu)       | 通过http/grpc协议，接收OTLP数据。                               |
| [`service_pgsql`](extended/service-pgsql.md)<br>PostgreSQL查询数据                   | SLS官方                                                      | 将PostgresSQL数据输入到iLogtail。                            |
| [`service_syslog`](extended/service-syslog.md)<br>Syslog数据                       | SLS官方                                                      | 采集syslog数据。                                           |

## 插件特性对比

| 特性 | 原生插件 | 扩展插件 |
|------|---------|---------|
| 实现语言 | C++ | Golang |
| 性能表现 | 极致性能 | 性能适中 |
| 资源开销 | 极低开销 | 开销适中 |
| 功能覆盖 | 专注常见场景 | 广泛覆盖 |
| 开发难度 | 中等 | 较低 |

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
  - 原生Input插件: 可配合原生/扩展Processor插件使用，支持SPL插件
  - 扩展Input插件: 仅支持扩展Processor插件
  - 详细说明请参考[处理插件文档](../processor/processors.md)
