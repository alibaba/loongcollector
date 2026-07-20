# 概览

下文各表按插件 **Type（反引号内英文名）字典序** 升序排列。

## 事件能力列说明

下列说明适用于插件概览表中的事件能力列（随插件演进持续更新）：

**占位符语义**

- `✓` = 支持（Yes）；`—` = 不支持 / 未实现（No）。

**原生 Input**

- **Log / Metric / Span**：Native Input 产出的 `PipelineEvent` 类型。

**扩展 Input**

- **Log / Metric / Span**：插件采集/接收后进入 Pipeline 的事件类型（v1 `protocol.Log` 或 v2 `models.*` 均按语义归类）。

**原生 Processor / 原生 Flusher**

- **Log / Metric / Span**：基于 `IsSupportedEvent` 或序列化/路由路径，插件明确支持处理的 `PipelineEvent` 类型。

**扩展 Processor / 扩展 Flusher / Aggregator / Extension**

- **v1 接口 / v2 接口**：是否实现 v1（`ProcessLogs` / `Flush(LogGroup)` / `Aggregator.Add`）或 v2（`Process(PipelineGroupEvents)` / `Export` / `FlushInterceptor.Intercept` 等）处理路径。
- **Log / Metric / Span**：对该事件类型的支持；`v1`/`v1&v2`/`v2` 表示明确实现，`透传` 表示 v2 `Process` 明确保留该类型事件（不处理但不丢弃），`透传未知` 表示代码中无类型特化信号（可能随 Pipeline 透传）。

## 输入

### 原生插件

> 各列含义见 [事件能力列说明](#事件能力列说明)。

| 名称 | 提供方 | Log | Metric | Span | 简介 |
| --- | --- | --- | --- | --- | --- |
| `input_agentsight`<br>[AgentSight](input/native/input_agentsight.md) | SLS 官方 | ✓ | — | — | AgentSight 可观测数据采集。 |
| `input_container_stdio`<br>[容器标准输出](input/native/input-container-stdio.md) | SLS 官方 | ✓ | — | — | 从容器标准输出/标准错误流中采集日志。 |
| `input_cpu_profiling`<br>[CPU Profiling](input/native/input-cpu-profiling.md) | SLS 官方 | ✓ | — | — | eBPF CPU Profiling 数据采集。 |
| `input_file`<br>[文本日志](input/native/input-file.md) | SLS 官方 | ✓ | — | — | 文本采集。 |
| `input_file_security`<br>[eBPF 文件安全数据](input/native/input-file-security.md) | SLS 官方 | ✓ | — | — | eBPF 文件安全数据采集。 |
| `input_forward`<br>[转发输入插件](input/native/input-forward.md) | SLS 官方 | ✓ | ✓ | ✓ | 接收转发数据；经 `processor_parse_from_pb_native` 解码后进入 Pipeline。 |
| `input_host_meta`<br>[主机元数据](input/native/input-host-meta.md) | SLS 官方 | ✓ | — | — | 定时采集主机、进程及关联关系等元数据。 |
| `input_host_monitor`<br>[主机监控数据](input/native/input-host-monitor.md) | SLS 官方 | — | ✓ | — | 采集主机 CPU/内存/磁盘/网络等指标。 |
| `input_internal_alarms`<br>[自监控告警数据](input/native/input-internal-alarms.md) | SLS 官方 | ✓ | — | — | 导出自监控告警数据。 |
| `input_internal_config_container_info`<br>(暂无文档) | SLS 官方 | ✓ | — | — | 导出容器配置快照。 |
| `input_internal_metrics`<br>[自监控指标数据](input/native/input-internal-metrics.md) | SLS 官方 | — | ✓ | — | 导出自监控指标数据。 |
| `input_network_observer`<br>[eBPF 网络可观测数据](input/native/input-network-observer.md) | SLS 官方 | ✓ | ✓ | ✓ | eBPF 网络可观测数据采集。 |
| `input_network_security`<br>[eBPF 网络安全数据](input/native/input-network-security.md) | SLS 官方 | ✓ | — | — | eBPF 网络安全数据采集。 |
| `input_process_security`<br>[eBPF 进程安全数据](input/native/input-process-security.md) | SLS 官方 | ✓ | — | — | eBPF 进程安全数据采集。 |
| `input_prometheus`<br>[Prometheus 抓取](input/native/input-prometheus.md) | SLS 官方 | — | ✓ | — | 按 ScrapeConfig 抓取指标。 |
| `input_static_file_onetime`<br>[一次性文件采集](input/native/input-static-file-onetime.md) | SLS 官方 | ✓ | — | — | 一次性文件采集。 |

### 扩展插件

> 各列含义见 [事件能力列说明](#事件能力列说明)。

| 名称 | 提供方 | Log | Metric | Span | 简介 |
| --- | --- | --- | --- | --- | --- |
| `input_command`<br>[脚本执行数据](input/extended/input-command.md) | 社区<br>[didachuxing](https://github.com/didachuxing) | ✓ | — | — | 采集脚本执行数据。 |
| `metric_debug_file`<br>[文本日志（debug）](input/extended/metric-debug-file.md) | SLS 官方 | ✓ | — | — | 用于调试的读取文件内容的插件。 |
| `metric_input_example`<br>[MetricInput 示例插件](input/extended/metric-input-example.md) | SLS 官方 | — | ✓ | — | MetricInput 示例插件。 |
| `metric_meta_host`<br>[主机 Meta 数据](input/extended/metric-meta-host.md) | SLS 官方 | — | ✓ | — | 主机 Meta 数据。 |
| `metric_mock`<br>[Mock 数据-Metric](input/extended/metric-mock.md) | SLS 官方 | — | ✓ | — | 生成 metric 模拟数据的插件。 |
| `metric_system_v2`<br>[主机监控数据](input/extended/metric-system.md) | SLS 官方 | — | ✓ | — | 主机监控数据。 |
| `service_canal`<br>[MySQL Binlog](input/extended/service-canal.md) | SLS 官方 | ✓ | — | — | 将 MySQL Binlog 输入到 LoongCollector。 |
| `service_docker_stdout`<br>[容器标准输出](input/extended/service-docker-stdout.md) | SLS 官方 | ✓ | — | — | 从容器标准输出/标准错误流中采集日志。 |
| `service_etw`<br>[Windows ETW 数据](input/extended/service-etw.md) | SLS 官方 | ✓ | — | — | 采集 Windows ETW（Event Tracing for Windows）实时事件。 |
| `service_go_profile`<br>[GO Profile](input/extended/service-goprofile.md) | SLS 官方 | ✓ | — | — | 采集 Golang pprof 性能数据。 |
| `service_gpu_metric`<br>[GPU 数据](input/extended/service-gpu.md) | SLS 官方 | — | ✓ | — | 支持收集英伟达 GPU 指标。 |
| `service_http_server`<br>[HTTP 数据](input/extended/service-http-server.md) | SLS 官方 | ✓ | ✓ | ✓ | 接收来自 unix socket、http/https、tcp 的请求，并支持 sls 协议、otlp 等多种协议。 |
| `service_input_example`<br>[ServiceInput 示例插件](input/extended/service-input-example.md) | SLS 官方 | ✓ | — | — | ServiceInput 示例插件。 |
| `service_journal`<br>[Journal 数据](input/extended/service-journal.md) | SLS 官方 | ✓ | — | — | 从原始的二进制文件中采集 Linux 系统的 Journal（systemd）日志。 |
| `service_kafka`<br>[Kafka](input/extended/service-kafka.md) | SLS 官方 | ✓ | — | — | 将 Kafka 数据输入到 LoongCollector。 |
| `service_kubernetes_meta`<br>[Kubernetes 元数据](input/extended/service-kubernetesmeta-v2.md) | SLS 官方 | ✓ | — | — | 在集群内采集 K8s 资源元数据并提供查询接口（文档路径含 v2，插件 Type 仍为 `service_kubernetes_meta`）。 |
| `service_mock`<br>[Mock 数据-Service](input/extended/service-mock.md) | SLS 官方 | ✓ | — | — | 生成 service 模拟数据的插件。 |
| `service_mssql`<br>[SqlServer 查询数据](input/extended/service-mssql.md) | SLS 官方 | ✓ | — | — | 将 Sql Server 数据输入到 LoongCollector。 |
| `service_otlp`<br>[OTLP 数据](input/extended/service-otlp.md) | 社区<br>[Zhu Shunjia](https://github.com/shunjiazhu) | ✓ | ✓ | ✓ | 通过 http/grpc 协议，接收 OTLP 数据。 |
| `service_pgsql`<br>[PostgreSQL 查询数据](input/extended/service-pgsql.md) | SLS 官方 | ✓ | — | — | 将 PostgresSQL 数据输入到 LoongCollector。 |
| `service_snmp`<br>[收集 SNMP 协议机器信息](input/extended/service-snmp.md) | SLS 官方 | ✓ | — | — | 收集 SNMP 协议机器信息. |
| `service_syslog`<br>[Syslog 数据](input/extended/service-syslog.md) | SLS 官方 | ✓ | — | — | 采集 syslog 数据。 |

## 处理

### SPL 处理

| 名称                                                                 | 提供方   | 简介                  |
| -------------------------------------------------------------------- | -------- | --------------------- |
| `processor_spl`<br>[SPL 处理](processor/spl/processor-spl-native.md) | SLS 官方 | 通过 SPL 语言解析数据 |

### 原生插件

> 各列含义见 [事件能力列说明](#事件能力列说明)。

| 名称 | 提供方 | Log | Metric | Span | 简介 |
| --- | --- | --- | --- | --- | --- |
| `processor_desensitize_native`<br>[脱敏原生处理插件](processor/native/processor-desensitize-native.md) | SLS 官方 | ✓ | — | — | 对事件指定字段内容进行脱敏。 |
| `processor_filter_regex_native`<br>[过滤原生处理插件](processor/native/processor-filter-regex-native.md) | SLS 官方 | ✓ | — | — | 根据事件字段内容来过滤事件。 |
| `processor_parse_apsara_native`<br>[Apsara 解析](processor/native/processor-parse-apsara-native.md) | SLS 官方 | ✓ | — | — | 按飞天格式解析时间与基础字段，并抽取 key:value 字段。 |
| `processor_parse_delimiter_native`<br>[分隔符解析原生处理插件](processor/native/processor-parse-delimiter-native.md) | SLS 官方 | ✓ | — | — | 解析事件中分隔符格式字段内容并提取新字段。 |
| `processor_parse_json_native`<br>[Json 解析原生处理插件](processor/native/processor-parse-json-native.md) | SLS 官方 | ✓ | — | — | 解析事件中 Json 格式字段内容并提取新字段。 |
| `processor_parse_regex_native`<br>[正则解析原生处理插件](processor/native/processor-parse-regex-native.md) | SLS 官方 | ✓ | — | — | 通过正则匹配解析事件指定字段内容并提取新字段。 |
| `processor_parse_timestamp_native`<br>[时间解析原生处理插件](processor/native/processor-parse-timestamp-native.md) | SLS 官方 | ✓ | — | — | 解析事件中记录时间的字段，并将结果置为事件的 \_\_time\_\_ 字段。 |
| `processor_timestamp_filter_native`<br>[时间戳过滤原生处理插件](processor/native/processor-timestamp-filter-native.md) | SLS 官方 | ✓ | — | — | 根据时间戳过滤事件，丢弃不在指定时间范围内的事件。 |

### 扩展插件

> 各列含义见 [事件能力列说明](#事件能力列说明)。

| 名称 | 提供方 | v1 接口 | v2 接口 | Log | Metric | Span | 简介 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `processor_add_fields`<br>[添加字段](processor/extended/processor-add-fields.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 添加字段。 |
| `processor_anchor`<br>(暂无文档) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 通过起止锚点提取字段（v2 暂为显式透传）。 |
| `processor_appender`<br>[追加字段](processor/extended/processor-appender.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 追加字段。 |
| `processor_base64_decoding`<br>[Base64 解码](processor/extended/processor-base64-decoding.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 对字段值进行 Base64 解码。 |
| `processor_base64_encoding`<br>[Base64 编码](processor/extended/processor-base64-encoding.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 对字段值进行 Base64 编码。 |
| `processor_cloud_meta`<br>[添加云资产信息](processor/extended/processor-cloudmeta.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 为日志增加云平台元数据信息（v2 暂为显式透传）。 |
| `processor_csv`<br>(暂无文档) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 按 CSV 格式解析字段（v2 暂为显式透传）。 |
| `processor_default`<br>[原始数据](processor/extended/processor-default.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 不对数据任何操作，只是简单的数据透传。 |
| `processor_desensitize`<br>[数据脱敏](processor/extended/processor-desensitize.md) | SLS 官方<br>[Takuka0311](https://github.com/Takuka0311) | ✓ | ✓ | v1&v2 | 透传 | 透传 | 对敏感数据进行脱敏处理。 |
| `processor_dict_map`<br>[字段值映射处理](processor/extended/processor-dict-map.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 对指定字段的值查表映射. |
| `processor_drop`<br>[丢弃字段](processor/extended/processor-drop.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 丢弃字段。 |
| `processor_drop_last_key`<br>[丢弃最后字段](processor/extended/processor-drop-last-key.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 丢弃匹配到的最后一个字段。 |
| `processor_encrypt`<br>[字段加密](processor/extended/processor-encrypy.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 加密字段 |
| `processor_fields_with_conditions`<br>[条件字段处理](processor/extended/processor-fields-with-condition.md) | 社区<br>[pj1987111](https://github.com/pj1987111) | ✓ | ✓ | 透传 | 透传 | 透传 | 根据日志部分字段的取值，动态进行字段扩展或删除（v2 暂为显式透传）。 |
| `processor_filter_key_regex`<br>(暂无文档) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 按字段名正则匹配过滤日志（v2 暂为显式透传）。 |
| `processor_filter_regex`<br>[日志过滤](processor/extended/processor-filter-regex.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 通过正则匹配过滤日志（v2 暂为显式透传）。 |
| `processor_geoip`<br>(暂无文档) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 基于 IP 补充地理位置信息（v2 暂为显式透传）。 |
| `processor_gotime`<br>[时间提取（Go 时间格式）](processor/extended/processor-gotime.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 以 Go 语言时间格式解析原始日志中的时间字段（v2 暂为显式透传）。 |
| `processor_grok`<br>[Grok](processor/extended/processor-grok.md) | SLS 官方<br>[Takuka0311](https://github.com/Takuka0311) | ✓ | ✓ | 透传 | 透传 | 透传 | 通过 Grok 语法对数据进行处理（v2 暂为显式透传）。 |
| `processor_json`<br>[Json](processor/extended/processor-json.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 实现对 Json 格式日志的解析。 |
| `processor_log_to_sls_metric`<br>[日志转 sls metric](processor/extended/processor-log-to-sls-metric.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 将日志转 sls metric（v2 暂为显式透传）。 |
| `processor_md5`<br>[MD5](processor/extended/processor-md5.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 计算指定字段值的 MD5。 |
| `processor_otel_metric`<br>[otel Metric格式转换](processor/extended/processor-otel-metric.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | OpenTelemetry Metric 数据格式转换（v2 暂为显式透传）。 |
| `processor_otel_trace`<br>[otel Trace格式转换](processor/extended/processor-otel-trace.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | OpenTelemetry Trace 数据格式转换（v2 暂为显式透传）。 |
| `processor_packjson`<br>[字段打包](processor/extended/processor-packjson.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 可添加指定的字段（支持多个）以 JSON 格式打包成单个字段。 |
| `processor_pick_key`<br>(暂无文档) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 保留或剔除指定字段（v2 暂为显式透传，避免丢弃事件）。 |
| `processor_rate_limit`<br>[日志限速](processor/extended/processor-rate-limit.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 用于对日志进行限速处理，确保在设定的时间窗口内，具有相同索引值的日志条目的数量不超过预定的速率限制（v2 暂为显式透传）。 |
| `processor_regex`<br>[正则](processor/extended/processor-regex.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 通过正则匹配的模式实现文本日志的字段提取（v2 暂为显式透传）。 |
| `processor_rename`<br>[重命名字段](processor/extended/processor-rename.md) | SLS 官方 | ✓ | ✓ | v1&v2 | v2 | v2 | 重命名字段。 |
| `processor_split_char`<br>[分隔符](processor/extended/processor-delimiter.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 通过单字符的分隔符提取字段（v2 暂为显式透传）。 |
| `processor_split_key_value`<br>[键值对](processor/extended/processor-split-key-value.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 通过切分键值对的方式提取字段（v2 暂为显式透传）。 |
| `processor_split_log_regex`<br>[多行切分](processor/extended/processor-split-log-regex.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 实现多行日志（例如 Java 程序日志）的采集（v2 暂为显式透传）。 |
| `processor_split_log_string`<br>[日志字符串切分](processor/extended/processor-split-log-string.md) | SLS 官方 | ✓ | ✓ | v1&v2 | 透传 | 透传 | 按分隔符将单条日志切分为多条（Log 事件切分，Metric/Span 透传）。 |
| `processor_split_string`<br>[分隔符](processor/extended/processor-delimiter.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 通过多字符的分隔符提取字段（v2 暂为显式透传）。 |
| `processor_string_replace`<br>[字符串替换](processor/extended/processor-string-replace.md) | SLS 官方<br>[pj1987111](https://github.com/pj1987111) | ✓ | ✓ | v1&v2 | 透传 | 透传 | 通过全文匹配、正则匹配、去转义字符等方式对文本日志进行内容替换。 |
| `processor_strptime`<br>[时间提取（strptime 格式）](processor/extended/processor-strptime.md) | SLS 官方 | ✓ | ✓ | 透传 | 透传 | 透传 | 从指定字段中提取日志时间，时间格式为 [Linux strptime](http://man7.org/linux/man-pages/man3/strptime.3.html)（v2 暂为显式透传）。 |
## 聚合

> 各列含义见 [事件能力列说明](#事件能力列说明)。

| 名称 | 提供方 | v1 接口 | v2 接口 | Log | Metric | Span | 简介 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `aggregator_base`<br>[基础聚合](aggregator/aggregator-base.md) | SLS 官方 | ✓ | — | v1 | 透传未知 | 透传未知 | 对单条数据进行聚合 |
| `aggregator_content_value_group`<br>[按 Key 聚合](aggregator/aggregator-content-value-group.md) | 社区<br>[snakorse](https://github.com/snakorse) | ✓ | — | v1 | 透传未知 | 透传未知 | 按照指定的 Key 对采集到的数据进行分组聚合 |
| `aggregator_context`<br>[上下文聚合](aggregator/aggregator-context.md) | SLS 官方 | ✓ | — | v1 | 透传未知 | 透传未知 | 根据数据来源对单条数据进行聚合 |
| `aggregator_metadata_group`<br>[GroupMetadata 聚合](aggregator/aggregator-metadata-group.md) | 社区<br>[urnotsally](https://github.com/urnotsally) | ✓ | — | v1 | 透传未知 | 透传未知 | 按照指定的 Metadata Keys 对采集到的数据进行重新分组聚合 |

## 输出

### 原生插件

> 各列含义见 [事件能力列说明](#事件能力列说明)。

| 名称 | 提供方 | Log | Metric | Span | 简介 |
| --- | --- | --- | --- | --- | --- |
| `flusher_blackhole`<br>[黑洞](flusher/native/flusher-blackhole.md) | SLS 官方 | ✓ | ✓ | ✓ | 直接丢弃采集的事件，属于原生输出插件，主要用于测试。 |
| `flusher_file`<br>[本地文件](flusher/native/flusher-file.md) | SLS 官方 | ✓ | ✓ | ✓ | 将采集到的数据写到本地文件。 |
| `flusher_kafka_native`<br>[Kafka](flusher/native/flusher-kafka.md) | <br>[ChaoEcho](https://github.com/ChaoEcho) | ✓ | — | — | 将采集到的数据输出到 Kafka（C++ 实现）。 |
| `flusher_sls`<br>[SLS](flusher/native/flusher-sls.md) | SLS 官方 | ✓ | ✓ | ✓ | 将采集到的数据输出到 SLS。 |
| `router`<br>[多 Flusher 路由](flusher/native/router.md) | SLS 官方 | ✓ | ✓ | ✓ | 在原生处理链路与支持的 Flusher 上按事件类型或 Tag 分流。 |

### 扩展插件

> 各列含义见 [事件能力列说明](#事件能力列说明)。

| 名称 | 提供方 | v1 接口 | v2 接口 | Log | Metric | Span | 简介 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `flusher_clickhouse`<br>[ClickHouse](flusher/extended/flusher-clickhouse.md) | 社区<br>[kl7sn](https://github.com/kl7sn) | ✓ | ✓ | v1&v2 | 透传未知 | 透传未知 | 将采集到的数据输出到 ClickHouse。 |
| `flusher_doris`<br>[Apache Doris](flusher/extended/flusher-doris.md) | 社区 | ✓ | ✓ | v1&v2 | 透传未知 | 透传未知 | 通过 Stream Load 将数据写入 Apache Doris。 |
| `flusher_elasticsearch`<br>[ElasticSearch](flusher/extended/flusher-elasticsearch.md) | 社区<br>[joeCarf](https://github.com/joeCarf) | ✓ | ✓ | v1&v2 | 透传未知 | 透传未知 | 将采集到的数据输出到 ElasticSearch。 |
| `flusher_http`<br>[HTTP](flusher/extended/flusher-http.md) | 社区<br>[snakorse](https://github.com/snakorse) | ✓ | ✓ | v1&v2 | 透传未知 | 透传未知 | 将采集到的数据以 http 方式输出到指定的后端。 |
| `flusher_kafka`<br>[Kafka](flusher/extended/flusher-kafka.md) | 社区 | — | — | — | — | — | Deprecated，代码已移除，请使用下面的 flusher_kafka_v2。 |
| `flusher_kafka_v2`<br>[Kafka V2](flusher/extended/flusher-kafka-v2.md) | 社区<br>[shalousun](https://github.com/shalousun) | ✓ | ✓ | v1&v2 | 透传未知 | 透传未知 | 将采集到的数据输出到 Kafka。 |
| `flusher_loki`<br>[Loki](flusher/extended/flusher-loki.md) | 社区<br>[abingcbc](https://github.com/abingcbc) | ✓ | ✓ | v1&v2 | 透传未知 | 透传未知 | 将采集到的数据输出到 Loki。 |
| `flusher_otlp_log`<br>[OTLP 日志](flusher/extended/flusher-otlp.md) | 社区<br>[liuhaoyang](https://github.com/liuhaoyang) | ✓ | ✓ | v1&v2 | 透传未知 | 透传未知 | 将采集到的数据支持`OpenTelemetry log protocol`的后端。 |
| `flusher_prometheus`<br>[Prometheus](flusher/extended/flusher-prometheus.md) | 社区<br> | — | ✓ | — | v2 | — | 将采集到的数据，经过处理后，通过 http 格式发送到指定的 Prometheus RemoteWrite 地址。 |
| `flusher_pulsar`<br>[Pulsar](flusher/extended/flusher-pulsar.md) | 社区<br>[shalousun](https://github.com/shalousun) | ✓ | ✓ | v1&v2 | 透传未知 | 透传未知 | 将采集到的数据输出到 Pulsar。 |
| `flusher_stdout`<br>[标准输出/文件](flusher/extended/flusher-stdout.md) | SLS 官方 | ✓ | ✓ | v1&v2 | v2 | v2 | 将采集到的数据输出到标准输出或文件。 |

## 扩展

扩展插件（按 **Type** 字典序；角色说明见各篇文档）。

> 各列含义见 [事件能力列说明](#事件能力列说明)。

| 名称 | 提供方 | v1 接口 | v2 接口 | Log | Metric | Span | 简介 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ext_basicauth`<br>[Basic 认证](extension/ext-basicauth.md) | 社区<br>[snakorse](https://github.com/snakorse) | — | — | 透传未知 | 透传未知 | 透传未知 | ClientAuthenticator：为 HTTP 类 Flusher 提供 Basic 认证。 |
| `ext_default_decoder`<br>[默认 decoder](extension/ext-default-decoder.md) | 社区<br>[snakorse](https://github.com/snakorse) | — | — | 透传未知 | 透传未知 | 透传未知 | Decoder：将内置支持的 Format 以 Decoder 扩展形式封装。 |
| `ext_default_encoder`<br>[默认 encoder](extension/ext-default-encoder.md) | 社区<br>[yuanshuai.1900](https://github.com/aiops1900) | — | — | 透传未知 | 透传未知 | 透传未知 | Encoder：将内置支持的 Format 以 Encoder 扩展形式封装。 |
| `ext_groupinfo_filter`<br>[GroupInfo 过滤](extension/ext-groupinfo-filter.md) | 社区<br>[snakorse](https://github.com/snakorse) | — | ✓ | 透传未知 | 透传未知 | 透传未知 | FlushInterceptor：按 GroupInfo 筛选提交数据。 |
| `ext_request_breaker`<br>[请求熔断](extension/ext-request-breaker.md) | 社区<br>[snakorse](https://github.com/snakorse) | — | — | 透传未知 | 透传未知 | 透传未知 | RequestInterceptor：为 HTTP 类 Flusher 提供熔断能力。 |
