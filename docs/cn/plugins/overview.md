# 概览

## 输入

| 名称                                                                            | 提供方                                                        | 简介                                                    |
|-------------------------------------------------------------------------------|------------------------------------------------------------|-------------------------------------------------------|
| [`input_file`](input/input-file.md)<br> 文本日志                                      | SLS官方 | 文本采集。                                                 |
| [`input_container_stdio`](input/input-container-stdlog.md)<br> 容器标准输出（原生插件）                                      | SLS官方 | 从容器标准输出/标准错误流中采集日志。                                                 |
| [`input_observer_network`](input/metric-observer.md)<br>eBPF网络调用数据         | SLS官方                                                      | 支持从网络系统调用中收集四层网络调用，并借助网络解析模块，可以观测七层网络调用细节。 |
| [`input_command`](input/input-command.md)<br>脚本执行数据                           | 社区<br>[`didachuxing`](https://github.com/didachuxing)      | 采集脚本执行数据。                                             |
| [`input_docker_stdout`](input/service-docker-stdout.md)<br>容器标准输出             | SLS官方                                                      | 从容器标准输出/标准错误流中采集日志。                                   |
| [`metric_debug_file`](input/metric-debug-file.md)<br>文本日志（debug）              | SLS官方                                                      | 用于调试的读取文件内容的插件。                                       |
| [`metric_input_example`](input/metric-input-example.md)<br>MetricInput示例插件    | SLS官方                                                      | MetricInput示例插件。                                      |
| [`metric_meta_host`](input/metric-meta-host.md)<br>主机Meta数据                   | SLS官方                                                      | 主机Meta数据。                                             |
| [`metric_mock`](input/metric-mock.md)<br>Mock数据-Metric                        | SLS官方                                                      | 生成metric模拟数据的插件。                                      |
| [`metric_system_v2`](input/metric-system.md)<br>主机监控数据                        | SLS官方                                                      | 主机监控数据。                                               |
| [`service_canal`](input/service-canal.md)<br>MySQL Binlog                     | SLS官方                                                      | 将MySQL Binlog输入到iLogtail。                             |
| [`service_go_profile`](input/service-goprofile.md)<br>GO Profile              | SLS官方                                                      | 采集Golang pprof 性能数据。                                  |
| [`service_gpu_metric`](input/service-gpu.md)<br>GPU数据                         | SLS官方                                                      | 支持收集英伟达GPU指标。                                         |
| [`service_http_server`](input/service-http-server.md)<br>HTTP数据               | SLS官方                                                      | 接收来自unix socket、http/https、tcp的请求，并支持sls协议、otlp等多种协议。 |
| [`service_input_example`](input/service-input-example.md)<br>ServiceInput示例插件 | SLS官方                                                      | ServiceInput示例插件。                                     |
| [`service_journal`](input/service-journal.md)<br>Journal数据                    | SLS官方                                                      | 从原始的二进制文件中采集Linux系统的Journal（systemd）日志。               |
| [`service_kafka`](input/service-kafka.md)<br>Kafka                            | SLS官方                                                      | 将Kafka数据输入到iLogtail。                                  |
| [`service_mock`](input/service-mock.md)<br>Mock数据-Service                     | SLS官方                                                      | 生成service模拟数据的插件。                                     |
| [`service_mssql`](input/service-mssql.md)<br>SqlServer查询数据                    | SLS官方                                                      | 将Sql Server数据输入到iLogtail。                             |
| [`service_otlp`](input/service-otlp.md)<br>OTLP数据                             | 社区<br>[`Zhu Shunjia`](https://github.com/shunjiazhu)       | 通过http/grpc协议，接收OTLP数据。                               |
| [`service_pgsql`](input/service-pgsql.md)<br>PostgreSQL查询数据                   | SLS官方                                                      | 将PostgresSQL数据输入到iLogtail。                            |
| [`service_syslog`](input/service-syslog.md)<br>Syslog数据                       | SLS官方                                                      | 采集syslog数据。                                           |
| [`input_ebpf_file_security`](input/input-ebpf-file-security.md)<br> eBPF文件安全数据                                      | SLS官方 | eBPF文件安全数据采集。                                                 |
| [`input_ebpf_network_observer`](input/input-ebpf-network-observer.md)<br> eBPF网络可观测数据                                      | SLS官方 | eBPF网络可观测数据采集。                                                 |
| [`input_ebpf_network_security`](input/input-ebpf-network-security.md)<br> eBPF网络安全数据                                      | SLS官方 | eBPF网络安全数据采集。                                                 |
| [`input_ebpf_process_security`](input/input-ebpf-process-security.md)<br> eBPF进程安全数据                                      | SLS官方 | eBPF进程安全数据采集。                                                 |

## 处理

### 原生插件

| 名称 | 提供方 | 简介 |
| --- | ----- | ---- |
| [`processor_parse_regex_native`](processor/native/processor-parse-regex-native.md)<br>正则解析原生处理插件 | SLS官方 | 通过正则匹配解析事件指定字段内容并提取新字段。 |
| [`processor_parse_json_native`](processor/native/processor-parse-json-native.md)<br>Json解析原生处理插件 | SLS官方 | 解析事件中`Json`格式字段内容并提取新字段。 |
| [`processor_parse_delimiter_native`](processor/native/processor-parse-delimiter-native.md)<br>分隔符解析原生处理插件 | SLS官方 | 解析事件中分隔符格式字段内容并提取新字段。 |
| [`processor_parse_timestamp_native`](processor/native/processor-parse-timestamp-native.md)<br>时间解析原生处理插件 | SLS官方 | 解析事件中记录时间的字段，并将结果置为事件的__time__字段。 |
| [`processor_filter_regex_native`](processor/native/processor-filter-regex-native.md)<br>过滤原生处理插件 | SLS官方 | 根据事件字段内容来过滤事件。 |
| [`processor_desensitize_native`](processor/native/processor-desensitize-native.md)<br>脱敏原生处理插件 | SLS官方 | 对事件指定字段内容进行脱敏。 |

### 拓展插件

| 名称 | 提供方 | 简介 |
| --- | ----- | ---- |
| [`processor_add_fields`](processor/extended/extenprocessor-add-fields.md)<br>添加字段                          | SLS官方                                                  | 添加字段。                            |
| [`processor_cloud_meta`](processor/extended/processor-cloudmeta.md)<br>添加云资产信息                        | SLS官方                                                  | 为日志增加云平台元数据信息。                   |
| [`processor_default`](processor/extended/processor-default.md)<br>原始数据                                | SLS官方                                                  | 不对数据任何操作，只是简单的数据透传。              |
| [`processor_desensitize`](processor/extended/processor-desensitize.md)<br>数据脱敏                        | SLS官方<br>[`Takuka0311`](https://github.com/Takuka0311) | 对敏感数据进行脱敏处理。                     |
| [`processor_drop`](processor/extended/processor-drop.md)<br>丢弃字段                                      | SLS官方                                                  | 丢弃字段。                            |
| [`processor_encrypt`](processor/extended/processor-encrypy.md)<br>字段加密                                | SLS官方                                                  | 加密字段                             |
| [`processor_fields_with_conditions`](processor/extended/processor-fields-with-condition.md)<br>条件字段处理 | 社区<br>[`pj1987111`](https://github.com/pj1987111)      | 根据日志部分字段的取值，动态进行字段扩展或删除。         |
| [`processor_filter_regex`](processor/extended/processor-filter-regex.md)<br>日志过滤                      | SLS官方                                                  | 通过正则匹配过滤日志。                      |
| [`processor_gotime`](processor/extended/processor-gotime.md)<br>Gotime                                | SLS官方                                                  | 以 Go 语言时间格式解析原始日志中的时间字段。         |
| [`processor_grok`](processor/extended/processor-grok.md)<br>Grok                                      | SLS官方<br>[`Takuka0311`](https://github.com/Takuka0311) | 通过 Grok 语法对数据进行处理                |
| [`processor_json`](processor/extended/processor-json.md)<br>Json                                      | SLS官方                                                  | 实现对Json格式日志的解析。                  |
| [`processor_log_to_sls_metric`](processor/extended/processor-log-to-sls-metric.md)<br>日志转sls metric   | SLS官方                                                  | 将日志转sls metric                   |
| [`processor_regex`](processor/extended/processor-regex.md)<br>正则                                      | SLS官方                                                  | 通过正则匹配的模式实现文本日志的字段提取。            |
| [`processor_rename`](processor/extended/processor-rename.md)<br>重命名字段                                 | SLS官方                                                  | 重命名字段。                           |
| [`processor_split_char`](processor/extended/processor-delimiter.md)<br>分隔符                            | SLS官方                                                  | 通过单字符的分隔符提取字段。                   |
| [`processor_split_string`](processor/extended/processor-delimiter.md)<br>分隔符                          | SLS官方                                                  | 通过多字符的分隔符提取字段。                   |
| [`processor_split_key_value`](processor/extended/processor-split-key-value.md)<br>键值对                 | SLS官方                                                  | 通过切分键值对的方式提取字段。                  |
| [`processor_split_log_regex`](processor/extended/processor-split-log-regex.md)<br>多行切分                | SLS官方                                                  | 实现多行日志（例如Java程序日志）的采集。           |
| [`processor_string_replace`](processor/extended/processor-string-replace.md)<br>字符串替换                 | SLS官方<br>[`pj1987111`](https://github.com/pj1987111)   | 通过全文匹配、正则匹配、去转义字符等方式对文本日志进行内容替换。 |

## 聚合

| 名称                                                                               | 提供方                                                 | 简介                                |
|----------------------------------------------------------------------------------|-----------------------------------------------------|-----------------------------------|
| [`aggregator_content_value_group`](aggregator/aggregator-content-value-group.md) | 社区<br>[`snakorse`](https://github.com/snakorse)     | 按照指定的Key对采集到的数据进行分组聚合             |
| [`aggregator_metadata_group`](aggregator/aggregator-metadata-group.md)           | 社区<br>[`urnotsally`](https://github.com/urnotsally) | 按照指定的Metadata Keys对采集到的数据进行重新分组聚合 |

## 输出

| 名称                                                                           | 提供方                                                 | 简介                                        |
|------------------------------------------------------------------------------|-----------------------------------------------------|-------------------------------------------|
| [`flusher_kafka`](flusher/flusher-kafka.md)<br>Kafka                         | 社区                                                  | 将采集到的数据输出到Kafka。推荐使用下面的flusher_kafka_v2   |
| [`flusher_kafka_v2`](flusher/flusher-kafka_v2.md)<br>Kafka                   | 社区<br>[`shalousun`](https://github.com/shalousun)   | 将采集到的数据输出到Kafka。                          |
| [`flusher_sls`](flusher/flusher-sls.md)<br>SLS                               | SLS官方                                               | 将采集到的数据输出到SLS。                            |
| [`flusher_stdout`](flusher/flusher-stdout.md)<br>标准输出/文件                     | SLS官方                                               | 将采集到的数据输出到标准输出或文件。                        |
| [`flusher_otlp_log`](flusher/flusher-otlp.md)<br>OTLP日志                      | 社区<br>[`liuhaoyang`](https://github.com/liuhaoyang) | 将采集到的数据支持`Opentelemetry log protocol`的后端。 |
| [`flusher_http`](flusher/flusher-http.md)<br>HTTP                            | 社区<br>[`snakorse`](https://github.com/snakorse)     | 将采集到的数据以http方式输出到指定的后端。                   |
| [`flusher_pulsar`](flusher/flusher-pulsar.md)<br>Kafka                       | 社区<br>[`shalousun`](https://github.com/shalousun)   | 将采集到的数据输出到Pulsar。                         |
| [`flusher_clickhouse`](flusher/flusher-clickhouse.md)<br>ClickHouse          | 社区<br>[`kl7sn`](https://github.com/kl7sn)           | 将采集到的数据输出到ClickHouse。                     |
| [`flusher_elasticsearch`](flusher/flusher-elasticsearch.md)<br>ElasticSearch | 社区<br>[`joeCarf`](https://github.com/joeCarf)       | 将采集到的数据输出到ElasticSearch。                  |
| [`flusher_loki`](flusher/loki.md)<br>Loki                                    | 社区<br>[`abingcbc`](https://github.com/abingcbc)     | 将采集到的数据输出到Loki。                           |

## 扩展

### ClientAuthenticator

| 名称                                                        | 提供方                                             | 简介                             |
|-----------------------------------------------------------|-------------------------------------------------|--------------------------------|
| [`ext_basicauth`](extension/ext-basicauth.md)<br> Basic认证 | 社区<br>[`snakorse`](https://github.com/snakorse) | 为 http_flusher 插件提供 basic 认证能力 |

### FlushInterceptor

| 名称                                                                          | 提供方                                             | 简介                                        |
|-----------------------------------------------------------------------------|-------------------------------------------------|-------------------------------------------|
| [`ext_groupinfo_filter`](extension/ext-groupinfo-filter.md)<br> GroupInfo过滤 | 社区<br>[`snakorse`](https://github.com/snakorse) | 为 http_flusher 插件提供根据GroupInfo筛选最终提交数据的能力 |

### RequestInterceptor

| 名称                                                                  | 提供方                                             | 简介                        |
|---------------------------------------------------------------------|-------------------------------------------------|---------------------------|
| [`ext_request_breaker`](extension/ext-request-breaker.md)<br> 请求熔断器 | 社区<br>[`snakorse`](https://github.com/snakorse) | 为 http_flusher 插件提供请求熔断能力 |

### Decoder

| 名称                                                                         | 提供方                                             | 简介                          |
|----------------------------------------------------------------------------|-------------------------------------------------|-----------------------------|
| [`ext_default_decoder`](extension/ext-default-decoder.md)<br> 默认的decoder扩展 | 社区<br>[`snakorse`](https://github.com/snakorse) | 将内置支持的Format以Decoder扩展的形式封装 |
