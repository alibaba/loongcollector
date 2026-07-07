# 自监控指标/告警字段对照表

本文档提供 LoongCollector 自监控体系中所有指标（Metric）、标签（Label）、告警（Alarm）字段的完整对照表，覆盖 C++ 和 Go 两端的常量定义。

> **维护要求**：本表与代码中的常量定义构成契约关系，任何字段变更必须同步更新本文档和对应的契约单测。

---

## 1. 指标类别（Category）

| 类别名 | `__name__` 值 | 含义 | C++ 来源 | Go 来源 |
|--------|--------------|------|----------|---------|
| agent | `agent` | 进程级指标，反映 Agent 全局状态（CPU、内存、fd 等） | `AgentMetrics.cpp` | `metrics_constants_agent.go` |
| pipeline | `pipeline` | 采集配置级指标，反映单条 Pipeline 的数据吞吐 | `PipelineMetrics.cpp` | — |
| plugin | `plugin` | 插件级指标，反映单个 Input/Processor/Flusher 插件的运行状况 | `PluginMetrics.cpp` | `metrics_constants_plugin.go` |
| component | `component` | 组件级指标，反映 Pipeline 内部组件（Queue、Batcher 等）的运行状况 | `ComponentMetrics.cpp` | — |
| runner | `runner` | Runner 级指标，反映各 Runner（文件采集、发送、eBPF 等）的运行状况 | `RunnerMetrics.cpp` | `metrics_constants_runner.go` |

---

## 2. 标签（Label）对照表

### 2.1 通用标签

| C++ 常量 | Go 常量 | 字符串值 | 含义 | 适用类别 |
|----------|---------|----------|------|----------|
| `METRIC_LABEL_KEY_PROJECT` | `MetricLabelKeyProject` | `project` | SLS Project 名称 | agent, plugin |
| `METRIC_LABEL_KEY_PIPELINE_NAME` | `MetricLabelKeyPipelineName` | `pipeline_name` | 采集配置（Pipeline）名称 | pipeline, plugin, component |
| `METRIC_LABEL_KEY_LOGSTORE` | `MetricLabelKeyLogstore` | `logstore` | SLS Logstore 名称 | pipeline, plugin |
| `METRIC_LABEL_KEY_REGION` | — | `region` | SLS 所在地域 | pipeline |

### 2.2 Agent 标签

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_LABEL_KEY_INSTANCE_ID` | — | `instance_id` | Agent 实例唯一标识 |
| `METRIC_LABEL_KEY_START_TIME` | — | `start_time` | Agent 启动时间 |
| `METRIC_LABEL_KEY_HOSTNAME` | — | `hostname` | 主机名 |
| `METRIC_LABEL_KEY_OS` | — | `os` | 操作系统名称 |
| `METRIC_LABEL_KEY_OS_DETAIL` | — | `os_detail` | 操作系统详细信息（版本、内核等） |
| `METRIC_LABEL_KEY_VERSION` | — | `version` | Agent 版本号 |
| `METRIC_LABEL_KEY_UUID` | — | `uuid` | Agent UUID |
| `METRIC_LABEL_KEY_USER_DEFINED_ID` | — | `user_defined_id` | 用户自定义的 Agent 标识 |
| `METRIC_LABEL_KEY_ALIUIDS` | — | `aliuids` | 关联的阿里云账号 UID 列表 |
| `METRIC_LABEL_KEY_ECS_INSTANCE_ID` | — | `ecs_instance_id` | ECS 实例 ID |
| `METRIC_LABEL_KEY_ECS_REGION_ID` | — | `ecs_region_id` | ECS 所在地域 ID |
| `METRIC_LABEL_KEY_ECS_USER_ID` | — | `ecs_user_id` | ECS 所属用户 ID |
| `METRIC_LABEL_KEY_HOST_ID` | — | `host_id` | 主机 ID |

### 2.3 Plugin 标签

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_LABEL_KEY_PLUGIN_TYPE` | `MetricLabelKeyPluginType` | `plugin_type` | 插件类型名称（如 `input_file`） |
| `METRIC_LABEL_KEY_PLUGIN_ID` | `MetricLabelKeyPluginID` | `plugin_id` | 插件实例 ID |
| — | `MetricLabelKeyMetricCategory` | `metric_category` | 指标分类标记（用于区分 plugin/runner） |

### 2.4 Plugin Source 标签（文件采集）

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_LABEL_KEY_FILE_DEV` | — | `file_dev` | 采集文件所在设备号 |
| `METRIC_LABEL_KEY_FILE_INODE` | — | `file_inode` | 采集文件的 inode 号 |
| `METRIC_LABEL_KEY_FILE_NAME` | — | `file_name` | 采集文件名 |

### 2.5 Prometheus 标签

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_LABEL_KEY_JOB` | — | `job` | Prometheus 抓取 Job 名称 |
| `METRIC_LABEL_KEY_POD_NAME` | — | `pod_name` | Kubernetes Pod 名称 |
| `METRIC_LABEL_KEY_SERVICE_HOST` | — | `service_host` | Prometheus 抓取目标主机 |
| `METRIC_LABEL_KEY_SERVICE_PORT` | — | `service_port` | Prometheus 抓取目标端口 |
| `METRIC_LABEL_KEY_STATUS` | — | `status` | 抓取结果状态 |
| `METRIC_LABEL_KEY_INSTANCE` | — | `instance` | Prometheus 实例标识 |

### 2.6 eBPF 标签

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_LABEL_KEY_RECV_EVENT_STAGE` | — | `recv_event_stage` | eBPF 事件在管道中的接收阶段 |
| `METRIC_LABEL_KEY_EVENT_TYPE` | — | `event_type` | eBPF 事件类型 |
| `METRIC_LABEL_KEY_PARSER_PROTOCOL` | — | `parser_protocol` | 协议解析器使用的协议类型 |
| `METRIC_LABEL_KEY_PARSE_STATUS` | — | `parser_status` | 协议解析结果状态 |
| `METRIC_LABEL_KEY_EVENT_SOURCE` | — | `event_source` | 事件来源 |
| `METRIC_LABEL_KEY_META_TYPE` | — | `meta_type` | 元数据富化类型 |
| `METRIC_LABEL_KEY_RECORD_TYPE` | — | `record_type` | 记录类型（应用层/网络层） |

### 2.7 Component 标签

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_LABEL_KEY_COMPONENT_NAME` | — | `component_name` | Pipeline 内部组件名称 |
| `METRIC_LABEL_KEY_FLUSHER_PLUGIN_ID` | — | `flusher_plugin_id` | 关联的 Flusher 插件 ID |
| `METRIC_LABEL_KEY_QUEUE_TYPE` | — | `queue_type` | 队列类型 |
| `METRIC_LABEL_KEY_TARGET` | — | `target` | 目标地址 |
| `METRIC_LABEL_KEY_EXACTLY_ONCE_ENABLED` | — | `exactly_once_enabled` | 是否启用 Exactly-Once 语义 |
| `METRIC_LABEL_KEY_GROUP_BATCH_ENABLED` | — | `group_batch_enabled` | 是否启用分组批处理 |
| `METRIC_LABEL_KEY_SERVICE_ADDRESS` | — | `service_address` | 服务地址（如 HTTP Sink 地址） |

### 2.8 Runner 标签

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_LABEL_KEY_RUNNER_NAME` | `MetricLabelKeyRunnerName` | `runner_name` | Runner 名称 |
| `METRIC_LABEL_KEY_THREAD_NO` | — | `thread_no` | 线程编号 |
| — | `MetricLabelKeyClusterID` | `cluster_id` | Kubernetes 集群 ID |

---

## 3. 标签值（Label Value）对照表

### 3.1 Component Name

| C++ 常量 | 字符串值 | 含义 |
|----------|----------|------|
| `METRIC_LABEL_VALUE_COMPONENT_NAME_BATCHER` | `batcher` | 事件批处理器 |
| `METRIC_LABEL_VALUE_COMPONENT_NAME_COMPRESSOR` | `compressor` | 数据压缩器 |
| `METRIC_LABEL_VALUE_COMPONENT_NAME_PROCESS_QUEUE` | `process_queue` | 处理队列 |
| `METRIC_LABEL_VALUE_COMPONENT_NAME_ROUTER` | `router` | 事件路由器 |
| `METRIC_LABEL_VALUE_COMPONENT_NAME_SENDER_QUEUE` | `sender_queue` | 发送队列 |
| `METRIC_LABEL_VALUE_COMPONENT_NAME_SERIALIZER` | `serializer` | 数据序列化器 |

### 3.2 Runner Name

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_LABEL_VALUE_RUNNER_NAME_FILE_SERVER` | — | `file_server` | 文件采集服务（文件发现与读取） |
| `METRIC_LABEL_VALUE_RUNNER_NAME_FLUSHER` | — | `flusher_runner` | 数据发送 Runner |
| `METRIC_LABEL_VALUE_RUNNER_NAME_HTTP_SINK` | — | `http_sink` | HTTP Sink Runner |
| `METRIC_LABEL_VALUE_RUNNER_NAME_PROCESSOR` | — | `processor_runner` | 数据处理 Runner |
| `METRIC_LABEL_VALUE_RUNNER_NAME_PROMETHEUS` | — | `prometheus_runner` | Prometheus 抓取 Runner |
| `METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER` | — | `ebpf_runner` | eBPF 采集 Runner |
| `METRIC_LABEL_VALUE_RUNNER_NAME_K8S_METADATA` | — | `k8s_metadata_runner` | Kubernetes 元数据富化 Runner |
| `METRIC_LABEL_VALUE_RUNNER_NAME_STATIC_FILE_SERVER` | — | `static_file_server` | 静态文件采集服务 |
| — | `MetricLabelValueRunnerNameK8sMeta` | `k8s_meta` | K8s 资源元信息 Runner |

### 3.3 Metric Category

| Go 常量 | 字符串值 | 含义 |
|---------|----------|------|
| `MetricLabelValueMetricCategoryPlugin` | `plugin` | 指标属于插件类别 |
| `MetricLabelValueMetricCategoryRunner` | `runner` | 指标属于 Runner 类别 |

### 3.4 eBPF Label Values

| C++ 常量 | 字符串值 | 含义 |
|----------|----------|------|
| `METRIC_LABEL_VALUE_RECV_EVENT_STAGE_POLL_KERNEL` | `poll_kernel` | 阶段：从内核轮询事件 |
| `METRIC_LABEL_VALUE_RECV_EVENT_STAGE_AFTER_PERF_WORKER` | `after_perf_worker` | 阶段：perf worker 处理后 |
| `METRIC_LABEL_VALUE_RECV_EVENT_STAGE_REPORT_TO_LC` | `report_to_lc` | 阶段：上报给 LoongCollector |
| `METRIC_LABEL_VALUE_EVENT_SOURCE_EBPF` | `ebpf` | 事件源：eBPF 子系统 |
| `METRIC_LABEL_VALUE_RECORD_TYPE_APP` | `app` | 记录类型：应用层 |
| `METRIC_LABEL_VALUE_RECORD_TYPE_NET` | `net` | 记录类型：网络层 |
| `METRIC_LABEL_VALUE_EVENT_TYPE_CONN_STATS` | `conn_stats` | 事件类型：连接统计 |
| `METRIC_LABEL_VALUE_EVENT_TYPE_DATA_EVENT` | `data_event` | 事件类型：数据事件 |
| `METRIC_LABEL_VALUE_EVENT_TYPE_CTRL_EVENT` | `ctrl_event` | 事件类型：控制事件 |
| `METRIC_LABEL_VALUE_EVENT_TYPE_LOG` | `log` | 事件类型：日志 |
| `METRIC_LABEL_VALUE_EVENT_TYPE_METRIC` | `metric` | 事件类型：指标 |
| `METRIC_LABEL_VALUE_EVENT_TYPE_TRACE` | `trace` | 事件类型：链路追踪 |
| `METRIC_LABEL_VALUE_PARSER_PROTOCOL_HTTP` | `http` | 解析协议：HTTP |
| `METRIC_LABEL_VALUE_PARSE_STATUS_SUCCESS` | `success` | 解析状态：成功 |
| `METRIC_LABEL_VALUE_PARSE_STATUS_FAILED` | `failed` | 解析状态：失败 |
| `METRIC_LABEL_VALUE_PLUGIN_TYPE_NETWORK_OBSERVER` | `network_observer` | eBPF 插件类型：网络观测 |
| `METRIC_LABEL_VALUE_PLUGIN_TYPE_NETWORK_SECURITY` | `network_security` | eBPF 插件类型：网络安全 |
| `METRIC_LABEL_VALUE_PLUGIN_TYPE_FILE_OBSERVER` | `file_observer` | eBPF 插件类型：文件观测 |
| `METRIC_LABEL_VALUE_PLUGIN_TYPE_FILE_SECURITY` | `file_security` | eBPF 插件类型：文件安全 |
| `METRIC_LABEL_VALUE_PLUGIN_TYPE_PROCESS_OBSERVER` | `process_observer` | eBPF 插件类型：进程观测 |
| `METRIC_LABEL_VALUE_PLUGIN_TYPE_PROCESS_SECURITY` | `process_security` | eBPF 插件类型：进程安全 |
| `METRIC_LABEL_VALUE_META_TYPE_NET` | `net` | 元数据类型：网络层 |
| `METRIC_LABEL_VALUE_META_TYPE_L7` | `l7` | 元数据类型：应用协议层（L7） |
| `METRIC_LABEL_VALUE_META_TYPE_K8S_SELF` | `k8s_self` | 元数据类型：本 Pod 的 K8s 信息 |
| `METRIC_LABEL_VALUE_META_TYPE_K8S_PEER` | `k8s_peer` | 元数据类型：对端 Pod 的 K8s 信息 |

---

## 4. 指标（Metric）对照表

### 4.1 通用指标（跨类别复用）

以下指标在 plugin、component、runner 类别中通过引用 `MetricCommonConstants` 复用同一字符串值。

| C++ 基础常量 | 字符串值 | 含义 | plugin 别名 | component 别名 | runner 别名 |
|-------------|----------|------|-------------|----------------|-------------|
| `METRIC_IN_EVENTS_TOTAL` | `in_events_total` | 接收的事件总数 | `METRIC_PLUGIN_IN_EVENTS_TOTAL` | `METRIC_COMPONENT_IN_EVENTS_TOTAL` | `METRIC_RUNNER_IN_EVENTS_TOTAL` |
| `METRIC_IN_EVENT_GROUPS_TOTAL` | `in_event_groups_total` | 接收的事件组总数 | `METRIC_PLUGIN_IN_EVENT_GROUPS_TOTAL` | — | `METRIC_RUNNER_IN_EVENT_GROUPS_TOTAL` |
| `METRIC_IN_SIZE_BYTES` | `in_size_bytes` | 接收的数据总字节数 | `METRIC_PLUGIN_IN_SIZE_BYTES` | `METRIC_COMPONENT_IN_SIZE_BYTES` | `METRIC_RUNNER_IN_SIZE_BYTES` |
| `METRIC_IN_ITEMS_TOTAL` | `in_items_total` | 接收的条目总数 | — | `METRIC_COMPONENT_IN_ITEMS_TOTAL` | `METRIC_RUNNER_IN_ITEMS_TOTAL` |
| `METRIC_OUT_EVENTS_TOTAL` | `out_events_total` | 输出的事件总数 | `METRIC_PLUGIN_OUT_EVENTS_TOTAL` | `METRIC_COMPONENT_OUT_EVENTS_TOTAL` | — |
| `METRIC_OUT_EVENT_GROUPS_TOTAL` | `out_event_groups_total` | 输出的事件组总数 | `METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL` | — | — |
| `METRIC_OUT_SIZE_BYTES` | `out_size_bytes` | 输出的数据总字节数 | `METRIC_PLUGIN_OUT_SIZE_BYTES` | `METRIC_COMPONENT_OUT_SIZE_BYTES` | `METRIC_RUNNER_OUT_SIZE_BYTES` |
| `METRIC_OUT_ITEMS_TOTAL` | `out_items_total` | 输出的条目总数 | — | `METRIC_COMPONENT_OUT_ITEMS_TOTAL` | `METRIC_RUNNER_OUT_ITEMS_TOTAL` |
| `METRIC_TOTAL_DELAY_MS` | `total_delay_ms` | 总延迟时间（毫秒） | `METRIC_PLUGIN_TOTAL_DELAY_MS` | `METRIC_COMPONENT_TOTAL_DELAY_MS` | `METRIC_RUNNER_TOTAL_DELAY_MS` |
| `METRIC_TOTAL_PROCESS_TIME_MS` | `total_process_time_ms` | 总处理耗时（毫秒） | `METRIC_PLUGIN_TOTAL_PROCESS_TIME_MS` | `METRIC_COMPONENT_TOTAL_PROCESS_TIME_MS` | — |
| `METRIC_DISCARDED_EVENTS_TOTAL` | `discarded_events_total` | 丢弃的事件总数 | `METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL` | — | — |
| `METRIC_DISCARDED_ITEMS_TOTAL` | `discarded_items_total` | 丢弃的条目总数 | — | `METRIC_COMPONENT_DISCARDED_ITEMS_TOTAL` | — |
| `METRIC_DISCARDED_SIZE_BYTES` | `discarded_size_bytes` | 丢弃的数据总字节数 | — | `METRIC_COMPONENT_DISCARDED_SIZE_BYTES` | — |

### 4.2 Agent 指标

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_AGENT_CPU` | — | `cpu` | Agent 进程 CPU 使用率 |
| `METRIC_AGENT_MEMORY` | — | `memory_used_mb` | Agent 进程内存占用（MB） |
| `METRIC_AGENT_MEMORY_GO` | `MetricAgentMemoryGo` | `go_memory_used_mb` | Go 插件层内存占用（MB） |
| `METRIC_AGENT_GO_ROUTINES_TOTAL` | `MetricAgentGoRoutinesTotal` | `go_routines_total` | Go 协程总数 |
| `METRIC_AGENT_OPEN_FD_TOTAL` | — | `open_fd_total` | 已打开的文件描述符总数 |
| `METRIC_AGENT_PIPELINE_CONFIG_TOTAL` | — | `pipeline_config_total` | 已加载的 Pipeline 配置总数 |
| `METRIC_AGENT_INSTANCE_CONFIG_TOTAL` | — | `instance_config_total` | 已加载的实例配置总数 |
| `METRIC_AGENT_HOST_MONITOR_TOTAL` | — | `host_monitor_config_total` | 已加载的主机监控配置总数 |

### 4.3 Pipeline 指标

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PIPELINE_PROCESSORS_IN_EVENTS_TOTAL` | — | `processor_in_events_total` | 进入 Processor 阶段的事件总数 |
| `METRIC_PIPELINE_PROCESSORS_IN_EVENT_GROUPS_TOTAL` | — | `processor_in_event_groups_total` | 进入 Processor 阶段的事件组总数 |
| `METRIC_PIPELINE_PROCESSORS_IN_SIZE_BYTES` | — | `processor_in_size_bytes` | 进入 Processor 阶段的数据总字节数 |
| `METRIC_PIPELINE_PROCESSORS_TOTAL_PROCESS_TIME_MS` | — | `processor_total_process_time_ms` | Processor 阶段总处理耗时（毫秒） |
| `METRIC_PIPELINE_FLUSHERS_IN_EVENTS_TOTAL` | — | `flusher_in_events_total` | 进入 Flusher 阶段的事件总数 |
| `METRIC_PIPELINE_FLUSHERS_IN_EVENT_GROUPS_TOTAL` | — | `flusher_in_event_groups_total` | 进入 Flusher 阶段的事件组总数 |
| `METRIC_PIPELINE_FLUSHERS_IN_SIZE_BYTES` | — | `flusher_in_size_bytes` | 进入 Flusher 阶段的数据总字节数 |
| `METRIC_PIPELINE_FLUSHERS_TOTAL_PACKAGE_TIME_MS` | — | `flusher_total_package_time_ms` | Flusher 阶段总打包耗时（毫秒） |
| `METRIC_PIPELINE_START_TIME` | — | `start_time` | Pipeline 启动时间 |

### 4.4 Plugin 通用指标

#### 所有插件通用

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_IN_EVENTS_TOTAL` | `MetricPluginInEventsTotal` | `in_events_total` | 插件接收的事件总数 |
| `METRIC_PLUGIN_IN_EVENT_GROUPS_TOTAL` | `MetricPluginInEventGroupsTotal` | `in_event_groups_total` | 插件接收的事件组总数 |
| `METRIC_PLUGIN_IN_SIZE_BYTES` | `MetricPluginInSizeBytes` | `in_size_bytes` | 插件接收的数据总字节数 |
| `METRIC_PLUGIN_OUT_EVENTS_TOTAL` | `MetricPluginOutEventsTotal` | `out_events_total` | 插件输出的事件总数 |
| `METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL` | `MetricPluginOutEventGroupsTotal` | `out_event_groups_total` | 插件输出的事件组总数 |
| `METRIC_PLUGIN_OUT_SIZE_BYTES` | `MetricPluginOutSizeBytes` | `out_size_bytes` | 插件输出的数据总字节数 |
| `METRIC_PLUGIN_TOTAL_DELAY_MS` | `MetricPluginTotalDelayMs` | `total_delay_ms` | 插件总延迟时间（毫秒） |
| `METRIC_PLUGIN_TOTAL_PROCESS_TIME_MS` | `MetricPluginTotalProcessTimeMs` | `total_process_time_ms` | 插件总处理耗时（毫秒） |

#### Processor 通用指标

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL` | `MetricPluginDiscardedEventsTotal` | `discarded_events_total` | Processor 丢弃的事件总数 |
| `METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL` | `MetricPluginOutFailedEventsTotal` | `out_failed_events_total` | Processor 输出失败的事件总数 |
| `METRIC_PLUGIN_OUT_KEY_NOT_FOUND_EVENTS_TOTAL` | `MetricPluginOutKeyNotFoundEventsTotal` | `out_key_not_found_events_total` | Processor 目标 Key 未找到的事件总数 |
| `METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL` | `MetricPluginOutSuccessfulEventsTotal` | `out_successful_events_total` | Processor 成功输出的事件总数 |
| `METRIC_PLUGIN_OUT_FAILED_EVENT_GROUPS_TOTAL` | — | `out_failed_event_groups_total` | Processor 输出失败的事件组总数 |
| `METRIC_PLUGIN_OUT_SUCCESSFUL_EVENT_GROUPS_TOTAL` | — | `out_successful_event_groups_total` | Processor 成功输出的事件组总数 |

#### Flusher 通用指标

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_FLUSHER_TOTAL_PACKAGE_TIME_MS` | — | `total_package_time_ms` | 数据打包总耗时（毫秒） |
| `METRIC_PLUGIN_FLUSHER_OUT_EVENT_GROUPS_TOTAL` | — | `send_total` | 发送请求总次数 |
| `METRIC_PLUGIN_FLUSHER_SEND_DONE_TOTAL` | — | `send_done_total` | 发送完成总次数（含成功与失败） |
| `METRIC_PLUGIN_FLUSHER_SUCCESS_TOTAL` | — | `success_total` | 发送成功总次数 |
| `METRIC_PLUGIN_FLUSHER_RETRY_TOTAL` | — | `retry_total` | 发送重试总次数 |
| `METRIC_PLUGIN_FLUSHER_DISCARD_TOTAL` | — | `discard_total` | 发送丢弃总次数（超过最大重试后丢弃） |
| `METRIC_PLUGIN_FLUSHER_NETWORK_ERROR_TOTAL` | — | `network_error_total` | 发送网络错误总次数 |
| `METRIC_PLUGIN_FLUSHER_SERVER_ERROR_TOTAL` | — | `server_error_total` | 发送服务端错误总次数 |
| `METRIC_PLUGIN_FLUSHER_UNAUTH_ERROR_TOTAL` | — | `unauth_error_total` | 发送鉴权错误总次数 |
| `METRIC_PLUGIN_FLUSHER_PARAMS_ERROR_TOTAL` | — | `params_error_total` | 发送参数错误总次数 |
| `METRIC_PLUGIN_FLUSHER_OTHER_ERROR_TOTAL` | — | `other_error_total` | 发送其他错误总次数 |

### 4.5 Plugin 专用指标

#### input_file / input_container_stdio

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_MONITOR_FILE_TOTAL` | — | `monitor_file_total` | 当前监控的文件总数 |
| `METRIC_PLUGIN_SOURCE_READ_OFFSET_BYTES` | — | `read_offset_bytes` | 源文件当前读取偏移量（字节） |
| `METRIC_PLUGIN_SOURCE_SIZE_BYTES` | — | `size_bytes` | 源文件大小（字节） |

#### input_prometheus

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_PROM_SUBSCRIBE_TARGETS` | — | `prom_subscribe_targets` | Prometheus 订阅目标数量 |
| `METRIC_PLUGIN_PROM_SUBSCRIBE_TOTAL` | — | `prom_subscribe_total` | Prometheus 订阅总次数 |
| `METRIC_PLUGIN_PROM_SUBSCRIBE_TIME_MS` | — | `prom_subscribe_time_ms` | Prometheus 订阅总耗时（毫秒） |
| `METRIC_PLUGIN_PROM_SCRAPE_TIME_MS` | — | `prom_scrape_time_ms` | Prometheus 抓取总耗时（毫秒） |
| `METRIC_PLUGIN_PROM_SCRAPE_DELAY_TOTAL` | — | `prom_scrape_delay_total` | Prometheus 抓取延迟总次数 |

#### input_ebpf / input_network_observer

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL` | — | `ebpf_loss_kernel_events_total` | eBPF 内核事件丢失总数 |
| `METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_CONNECTION_NUM` | — | `connection_num` | 当前跟踪的连接数 |
| `METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_WORKER_HANDLE_EVENTS_TOTAL` | — | `handle_events_total` | Worker 线程处理的事件总数 |
| `METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_PROTOCOL_PARSE_RECORDS_TOTAL` | — | `parse_records_total` | 协议解析的记录总数 |
| `METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_AGGREGATE_EVENTS_TOTAL` | — | `aggregate_events_total` | 聚合的事件总数 |
| `METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_AGGREGATE_KEY_NUM` | — | `aggregate_key_num` | 聚合键的数量 |
| `METRIC_PLUGIN_EBPF_PROCESS_CACHE_ENTRIES_NUM` | — | `process_cache_entries_num` | 进程缓存条目数 |
| `METRIC_PLUGIN_EBPF_PROCESS_CACHE_MISS_TOTAL` | — | `process_cache_miss_total` | 进程缓存未命中总数 |
| `METRIC_PLUGIN_EBPF_META_ATTACH_SUCCESS_TOTAL` | — | `meta_attach_success_total` | 元数据关联成功总数 |
| `METRIC_PLUGIN_EBPF_META_ATTACH_FAILED_TOTAL` | — | `meta_attach_failed_total` | 元数据关联失败总数 |
| `METRIC_PLUGIN_EBPF_META_ATTACH_ROLLBACK_TOTAL` | — | `meta_attach_rollback_total` | 元数据关联回退总数 |

#### processor_parse_apsara_native / processor_parse_timestamp_native

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_HISTORY_FAILURE_TOTAL` | — | `history_failure_total` | 历史时间戳解析失败总数 |

#### processor_split_multiline_log_string_native

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_MATCHED_EVENTS_TOTAL` | — | `matched_events_total` | 匹配多行模式的事件总数 |
| `METRIC_PLUGIN_MATCHED_LINES_TOTAL` | — | `matched_lines_total` | 匹配多行模式的行总数 |
| `METRIC_PLUGIN_UNMATCHED_LINES_TOTAL` | — | `unmatched_lines_total` | 未匹配多行模式的行总数 |

#### processor_merge_multiline_log_native

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_MERGED_EVENTS_TOTAL` | — | `merged_events_total` | 多行合并后的事件总数 |
| `METRIC_PLUGIN_UNMATCHED_EVENTS_TOTAL` | — | `unmatched_events_total` | 多行合并中未匹配的事件总数 |

#### processor_parse_container_log_native

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_PARSE_STDERR_TOTAL` | — | `parse_stderr_total` | 解析的容器 stderr 行总数 |
| `METRIC_PLUGIN_PARSE_STDOUT_TOTAL` | — | `parse_stdout_total` | 解析的容器 stdout 行总数 |

#### flusher_sls

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_FLUSHER_SLS_SHARD_WRITE_QUOTA_ERROR_TOTAL` | — | `shard_write_quota_error_total` | SLS Shard 写入配额超限错误总数 |
| `METRIC_PLUGIN_FLUSHER_SLS_PROJECT_QUOTA_ERROR_TOTAL` | — | `project_quota_error_total` | SLS Project 配额超限错误总数 |
| `METRIC_PLUGIN_FLUSHER_SLS_SEQUENCE_ID_ERROR_TOTAL` | — | `sequence_id_error_total` | SLS Sequence ID 错误总数 |
| `METRIC_PLUGIN_FLUSHER_SLS_REQUEST_EXPRIRED_ERROR_TOTAL` | — | `request_exprired_error_total` | SLS 请求过期错误总数 |

#### input_canal (Go only)

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| — | `MetricPluginBinlogRotate` | `binlog_rotate` | Binlog 文件轮转次数 |
| — | `MetricPluginBinlogSync` | `binlog_sync` | Binlog 同步次数 |
| — | `MetricPluginBinlogDdl` | `binlog_ddl` | Binlog DDL 事件次数 |
| — | `MetricPluginBinlogRow` | `binlog_row` | Binlog 行事件次数 |
| — | `MetricPluginBinlogXgid` | `binlog_xgid` | Binlog 事务组 ID 次数 |
| — | `MetricPluginBinlogCheckpoint` | `binlog_checkpoint` | Binlog Checkpoint 位点 |
| — | `MetricPluginBinlogFilename` | `binlog_filename` | 当前 Binlog 文件名 |
| — | `MetricPluginBinlogGtid` | `binlog_gtid` | 当前 Binlog GTID |

#### metric_container_info / service_docker_stdout_v2 (Go only)

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| — | `MetricPluginContainerTotal` | `container_total` | 当前监控的容器总数 |
| — | `MetricPluginAddContainerTotal` | `add_container_total` | 新增容器总数 |
| — | `MetricPluginRemoveContainerTotal` | `remove_container_total` | 移除容器总数 |
| — | `MetricPluginUpdateContainerTotal` | `update_container_total` | 更新容器总数 |

#### service_mysql / service_rdb (Go only)

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| — | `MetricPluginCollectAvgCostTimeMs` | `collect_avg_cost_time_ms` | 平均采集耗时（毫秒） |
| — | `MetricPluginCollectTotal` | `collect_total` | 采集总次数 |

#### service_k8s_meta (Go only)

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| — | `MetricCollectEntityTotal` | `collect_entity_total` | 采集的实体总数 |
| — | `MetricCollectLinkTotal` | `collect_link_total` | 采集的关联关系总数 |

#### processor_anchor / processor_regex / processor_string_replace (Go only)

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| — | `PluginPairsPerLogTotal` | `pairs_per_log_total` | 每条日志提取的键值对总数 |

#### host_monitor collector fail metrics

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_PLUGIN_CPU_FAIL_TOTAL` | — | `cpu_fail_total` | CPU 指标采集失败总数 |
| `METRIC_PLUGIN_SYSTEM_FAIL_TOTAL` | — | `system_fail_total` | 系统指标采集失败总数 |
| `METRIC_PLUGIN_MEM_FAIL_TOTAL` | — | `mem_fail_total` | 内存指标采集失败总数 |
| `METRIC_PLUGIN_NET_FAIL_TOTAL` | — | `net_fail_total` | 网络指标采集失败总数 |
| `METRIC_PLUGIN_PROCESS_FAIL_TOTAL` | — | `process_fail_total` | 进程指标采集失败总数 |
| `METRIC_PLUGIN_DISK_FAIL_TOTAL` | — | `disk_fail_total` | 磁盘指标采集失败总数 |

### 4.6 Component 指标

#### Batcher

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_COMPONENT_BATCHER_EVENT_BATCHES_TOTAL` | — | `event_batches_total` | 创建的事件批次总数 |
| `METRIC_COMPONENT_BATCHER_BUFFERED_GROUPS_TOTAL` | — | `buffered_groups_total` | 当前缓冲的事件组数 |
| `METRIC_COMPONENT_BATCHER_BUFFERED_EVENTS_TOTAL` | — | `buffered_events_total` | 当前缓冲的事件数 |
| `METRIC_COMPONENT_BATCHER_BUFFERED_SIZE_BYTES` | — | `buffered_size_bytes` | 当前缓冲的数据大小（字节） |
| `METRIC_COMPONENT_BATCHER_TOTAL_ADD_TIME_MS` | — | `total_add_time_ms` | 添加到批次的总耗时（毫秒） |

#### Queue

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_COMPONENT_QUEUE_SIZE` | — | `queue_size` | 当前队列大小（条目数） |
| `METRIC_COMPONENT_QUEUE_SIZE_BYTES` | — | `queue_size_bytes` | 当前队列大小（字节） |
| `METRIC_COMPONENT_QUEUE_VALID_TO_PUSH_FLAG` | — | `valid_to_push_status` | 队列是否可接受新数据 |
| `METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE` | — | `extra_buffer_size` | 额外缓冲区大小（条目数） |
| `METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE_BYTES` | — | `extra_buffer_size_bytes` | 额外缓冲区大小（字节） |
| `METRIC_COMPONENT_QUEUE_DISCARDED_EVENTS_TOTAL` | — | `discarded_events_total` | 队列丢弃的事件总数 |
| `METRIC_COMPONENT_QUEUE_FETCHED_ITEMS_TOTAL` | — | `fetched_items_total` | 从队列取出的条目总数 |
| `METRIC_COMPONENT_QUEUE_FETCH_TIMES_TOTAL` | — | `fetch_times_total` | 队列取数据总次数 |
| `METRIC_COMPONENT_QUEUE_VALID_FETCH_TIMES_TOTAL` | — | `valid_fetch_times_total` | 队列成功取数据的次数 |
| `METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_REGION_LIMITER_TIMES_TOTAL` | — | `region_reject_times_total` | 被地域限流器拒绝的取数据次数 |
| `METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_PROJECT_LIMITER_TIMES_TOTAL` | — | `project_reject_times_total` | 被 Project 限流器拒绝的取数据次数 |
| `METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_LOGSTORE_LIMITER_TIMES_TOTAL` | — | `logstore_reject_times_total` | 被 Logstore 限流器拒绝的取数据次数 |
| `METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_RATE_LIMITER_TIMES_TOTAL` | — | `rate_reject_times_total` | 被通用限流器拒绝的取数据次数 |

### 4.7 Runner 指标

#### Runner 通用

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_LAST_RUN_TIME` | — | `last_run_time` | Runner 上次执行时间 |
| `METRIC_RUNNER_CLIENT_REGISTER_STATE` | — | `client_register_state` | 客户端注册状态 |
| `METRIC_RUNNER_CLIENT_REGISTER_RETRY_TOTAL` | — | `client_register_retry_total` | 客户端注册重试总次数 |
| `METRIC_RUNNER_JOBS_TOTAL` | — | `jobs_total` | Runner 处理的任务总数 |

#### Sink 通用

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_SINK_OUT_SUCCESSFUL_ITEMS_TOTAL` | — | `out_successful_items_total` | 发送成功的条目总数 |
| `METRIC_RUNNER_SINK_OUT_FAILED_ITEMS_TOTAL` | — | `out_failed_items_total` | 发送失败的条目总数 |
| `METRIC_RUNNER_SINK_SUCCESSFUL_ITEM_TOTAL_RESPONSE_TIME_MS` | — | `successful_response_time_ms` | 发送成功的总响应时间（毫秒） |
| `METRIC_RUNNER_SINK_FAILED_ITEM_TOTAL_RESPONSE_TIME_MS` | — | `failed_response_time_ms` | 发送失败的总响应时间（毫秒） |
| `METRIC_RUNNER_SINK_SENDING_ITEMS_TOTAL` | — | `sending_items_total` | 当前正在发送的条目数 |
| `METRIC_RUNNER_SINK_SEND_CONCURRENCY` | — | `send_concurrency` | 当前发送并发度 |

#### Flusher Runner

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_FLUSHER_IN_RAW_SIZE_BYTES` | — | `in_raw_size_bytes` | 接收的原始（未压缩）数据大小（字节） |
| `METRIC_RUNNER_FLUSHER_OUT_RAW_SIZE_BYTES` | — | `out_raw_size_bytes` | 输出的原始（未压缩）数据大小（字节） |
| `METRIC_RUNNER_FLUSHER_WAITING_ITEMS_TOTAL` | — | `waiting_items_total` | 等待发送的条目总数 |

#### File Server

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_FILE_WATCHED_DIRS_TOTAL` | — | `watched_dirs_total` | 当前监控的目录总数 |
| `METRIC_RUNNER_FILE_ACTIVE_READERS_TOTAL` | — | `active_readers_total` | 当前活跃的文件 Reader 总数 |
| `METRIC_RUNNER_FILE_ENABLE_FILE_INCLUDED_BY_MULTI_CONFIGS_FLAG` | — | `enable_multi_configs` | 是否允许文件被多个配置采集 |
| `METRIC_RUNNER_FILE_POLLING_MODIFY_CACHE_SIZE` | — | `polling_modify_cache_size` | 轮询修改事件缓存大小 |
| `METRIC_RUNNER_FILE_POLLING_DIR_CACHE_SIZE` | — | `polling_dir_cache_size` | 轮询目录缓存大小 |
| `METRIC_RUNNER_FILE_POLLING_FILE_CACHE_SIZE` | — | `polling_file_cache_size` | 轮询文件缓存大小 |

#### Static File Server

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_STATIC_FILE_SERVER_ACTIVE_INPUTS_COUNT` | — | `active_inputs_count` | 当前活跃的静态文件输入数 |

#### eBPF Server

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_EBPF_POLL_PROCESS_EVENTS_TOTAL` | — | `poll_process_events_total` | 轮询到的进程事件总数 |
| `METRIC_RUNNER_EBPF_LOSS_PROCESS_EVENTS_TOTAL` | — | `loss_process_events_total` | 丢失的进程事件总数 |
| `METRIC_RUNNER_EBPF_PROCESS_CACHE_MISS_TOTAL` | — | `process_cache_miss_total` | 进程缓存未命中总数 |
| `METRIC_RUNNER_EBPF_PROCESS_CACHE_SIZE` | — | `process_cache_size` | 进程缓存大小 |
| `METRIC_RUNNER_EBPF_PROCESS_DATA_MAP_SIZE` | — | `process_data_map_size` | 进程数据 Map 大小 |
| `METRIC_RUNNER_EBPF_RETRYABLE_EVENT_CACHE_SIZE` | — | `retryable_event_cache_size` | 可重试事件缓存大小 |
| `METRIC_RUNNER_EBPF_POLL_KERNEL_EVENTS_TOTAL` | — | `poll_kernel_event_total` | 轮询到的内核事件总数 |
| `METRIC_RUNNER_EBPF_LOST_KERNEL_EVENTS_TOTAL` | — | `lost_kernel_event_total` | 丢失的内核事件总数 |
| `METRIC_RUNNER_EBPF_CONNECTION_CACHE_SIZE` | — | `connection_cache_size` | 连接缓存大小 |
| `METRIC_RUNNER_EBPF_LOST_LOG_EVENTS_TOTAL` | — | `lost_log_event_total` | 丢失的日志事件总数 |
| `METRIC_RUNNER_EBPF_CPU_PROFILING_PID_MATCH_CACHE_SIZE` | — | `cpu_profiling_pid_match_cache_size` | CPU Profiling PID 匹配缓存大小 |

#### K8s Metadata

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_METADATA_CID_CACHE_SIZE` | — | `cid_cache_size` | 容器 ID 缓存大小 |
| `METRIC_RUNNER_METADATA_IP_CACHE_SIZE` | — | `ip_cache_size` | IP 地址缓存大小 |
| `METRIC_RUNNER_METADATA_EXTERNAL_IP_CACHE_SIZE` | — | `external_ip_cache_size` | 外部 IP 缓存大小 |
| `METRIC_RUNNER_METADATA_REQUEST_REMOTE_TOTAL` | — | `request_metadata_server_total` | 请求元数据服务的总次数 |
| `METRIC_RUNNER_METADATA_REQUEST_REMOTE_FAILED_TOTAL` | — | `request_metadata_server_failed_total` | 请求元数据服务失败的总次数 |

#### K8s Meta (Go only)

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| — | `MetricRunnerK8sMetaAddEventTotal` | `add_event_total` | K8s 资源新增事件总数 |
| — | `MetricRunnerK8sMetaUpdateEventTotal` | `update_event_total` | K8s 资源更新事件总数 |
| — | `MetricRunnerK8sMetaDeleteEventTotal` | `delete_event_total` | K8s 资源删除事件总数 |
| — | `MetricRunnerK8sMetaCacheSize` | `cache_size` | K8s 元信息缓存大小 |
| — | `MetricRunnerK8sMetaQueueSize` | `queue_size` | K8s 事件队列大小 |
| — | `MetricRunnerK8sMetaHTTPRequestTotal` | `http_request_total` | K8s API HTTP 请求总数 |
| — | `MetricRunnerK8sMetaHTTPAvgDelayMs` | `avg_delay_ms` | K8s API 平均请求延迟（毫秒） |
| — | `MetricRunnerK8sMetaHTTPMaxDelayMs` | `max_delay_ms` | K8s API 最大请求延迟（毫秒） |

#### Timer

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_TIMER_OUT_ITEMS_TOTAL` | — | `out_items_total` | Timer 输出的条目总数 |
| `METRIC_RUNNER_TIMER_IN_ITEMS_TOTAL` | — | `in_items_total` | Timer 接收的条目总数 |
| `METRIC_RUNNER_TIMER_LATENCY_TIME_MS` | — | `latency_time_ms` | Timer 延迟时间（毫秒） |
| `METRIC_RUNNER_TIMER_QUEUE_ITEMS_TOTAL` | — | `queue_items_total` | Timer 队列中的条目总数 |

#### Host Monitor Runner

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_HOST_MONITOR_OUT_ITEMS_TOTAL` | — | `out_items_total` | 主机监控输出的条目总数 |
| `METRIC_RUNNER_HOST_MONITOR_OUT_ITEMS_SIZE` | — | `out_items_size` | 主机监控输出的条目大小 |
| `METRIC_RUNNER_HOST_MONITOR_DROP_ITEMS_TOTAL` | — | `drop_items_total` | 主机监控丢弃的条目总数 |
| `METRIC_RUNNER_HOST_MONITOR_LATENCY_TIME_MS` | — | `latency_time_ms` | 主机监控采集延迟（毫秒） |

#### System Interface

| C++ 常量 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `METRIC_RUNNER_SYSTEM_OP_TOTAL` | — | `system_op_total` | 系统操作总次数 |
| `METRIC_RUNNER_SYSTEM_OP_FAIL_TOTAL` | — | `system_op_fail_total` | 系统操作失败总次数 |
| `METRIC_RUNNER_SYSTEM_CACHE_HIT_TOTAL` | — | `cache_hit_total` | 系统缓存命中总次数 |
| `METRIC_RUNNER_SYSTEM_CACHE_ITEMS_SIZE` | — | `cache_items_size` | 系统缓存条目数 |

---

## 5. 告警（Alarm）对照表

### 5.1 告警级别

| C++ 枚举 | Go 常量 | 字符串值 | 含义 |
|----------|---------|----------|------|
| `ALARM_LEVEL_WARNING` (1) | `AlarmLevelWaring` | `1` | 单点报错，不影响整体流程 |
| `ALARM_LEVEL_ERROR` (2) | `AlarmLevelError` | `2` | 对主要流程有影响，如不优化处理可能导致风险 |
| `ALARM_LEVEL_CRITICAL` (3) | `AlarmLevelCritical` | `3` | 采集配置/重要模块不可用；对 Agent 稳定性造成影响；导致资损 |

### 5.2 C++ 告警类型

以下为 C++ 端 `AlarmType` 枚举的完整列表，定义于 `core/monitor/AlarmManager.h`。

| 枚举名 | 枚举值 | 消息类型字符串 | 含义 |
|--------|--------|----------------|------|
| `USER_CONFIG_ALARM` | 0 | `USER_CONFIG_ALARM` | 用户配置错误 |
| `GLOBAL_CONFIG_ALARM` | 1 | `GLOBAL_CONFIG_ALARM` | 全局配置错误 |
| `DOMAIN_SOCKET_BIND_ALARM` | 2 | `DOMAIN_SOCKET_BIND_ALARM` | Domain Socket 绑定失败 |
| `SECONDARY_READ_WRITE_ALARM` | 3 | `SECONDARY_READ_WRITE_ALARM` | 本地二级存储读写失败 |
| `LOGFILE_PERMINSSION_ALARM` | 4 | `LOGFILE_PERMINSSION_ALARM` | 日志文件权限不足 |
| `SEND_QUOTA_EXCEED_ALARM` | 5 | `SEND_QUOTA_EXCEED_ALARM` | 发送配额超限 |
| `LOGTAIL_CRASH_ALARM` | 6 | `LOGTAIL_CRASH_ALARM` | Agent 进程崩溃 |
| `INOTIFY_DIR_NUM_LIMIT_ALARM` | 7 | `INOTIFY_DIR_NUM_LIMIT_ALARM` | Inotify 监控目录数达到上限 |
| `EPOLL_ERROR_ALARM` | 8 | `EPOLL_ERROR_ALARM` | Epoll 错误 |
| `DISCARD_DATA_ALARM` | 9 | `DISCARD_DATA_ALARM` | 数据被丢弃 |
| `READ_LOG_DELAY_ALARM` | 10 | `READ_LOG_DELAY_ALARM` | 日志读取延迟（采集落后） |
| `MULTI_CONFIG_MATCH_ALARM` | 11 | `MULTI_CONFIG_MATCH_ALARM` | 多个配置匹配同一文件 |
| `REGISTER_INOTIFY_FAIL_ALARM` | 12 | `REGISTER_INOTIFY_FAIL_ALARM` | 注册 Inotify 监控失败 |
| `LOGTAIL_CONFIG_ALARM` | 13 | `LOGTAIL_CONFIG_ALARM` | Logtail 配置错误 |
| `ENCRYPT_DECRYPT_FAIL_ALARM` | 14 | `ENCRYPT_DECRYPT_FAIL_ALARM` | 加密/解密失败 |
| `LOG_GROUP_PARSE_FAIL_ALARM` | 15 | `LOG_GROUP_PARSE_FAIL_ALARM` | LogGroup 解析失败 |
| `METRIC_GROUP_PARSE_FAIL_ALARM` | 16 | `METRIC_GROUP_PARSE_FAIL_ALARM` | MetricGroup 解析失败 |
| `LOGDIR_PERMISSION_ALARM` | 17 | `LOGDIR_PERMISSION_ALARM` | 日志目录权限不足 |
| `REGEX_MATCH_ALARM` | 18 | `REGEX_MATCH_ALARM` | 正则匹配失败 |
| `DISCARD_SECONDARY_ALARM` | 19 | `DISCARD_SECONDARY_ALARM` | 二级存储数据被丢弃 |
| `BINARY_UPDATE_ALARM` | 20 | `BINARY_UPDATE_ALARM` | Agent 二进制更新 |
| `CONFIG_UPDATE_ALARM` | 21 | `CONFIG_UPDATE_ALARM` | 配置更新事件 |
| `CHECKPOINT_ALARM` | 22 | `CHECKPOINT_ALARM` | Checkpoint 异常 |
| `CATEGORY_CONFIG_ALARM` | 23 | `CATEGORY_CONFIG_ALARM` | 分类配置错误 |
| `INOTIFY_EVENT_OVERFLOW_ALARM` | 24 | `INOTIFY_EVENT_OVERFLOW_ALARM` | Inotify 事件队列溢出 |
| `INVALID_MEMORY_ACCESS_ALARM` | 25 | `INVALID_MEMORY_ACCESS_ALARM` | 非法内存访问 |
| `ENCODING_CONVERT_ALARM` | 26 | `ENCODING_CONVERT_ALARM` | 编码转换错误 |
| `SPLIT_LOG_FAIL_ALARM` | 27 | `SPLIT_LOG_FAIL_ALARM` | 日志分割失败 |
| `OPEN_LOGFILE_FAIL_ALARM` | 28 | `OPEN_LOGFILE_FAIL_ALARM` | 打开日志文件失败 |
| `SEND_DATA_FAIL_ALARM` | 29 | `SEND_DATA_FAIL_ALARM` | 数据发送失败 |
| `PARSE_TIME_FAIL_ALARM` | 30 | `PARSE_TIME_FAIL_ALARM` | 时间戳解析失败 |
| `OUTDATED_LOG_ALARM` | 31 | `OUTDATED_LOG_ALARM` | 检测到过期日志 |
| `STREAMLOG_TCP_SOCKET_BIND_ALARM` | 32 | `STREAMLOG_TCP_SOCKET_BIND_ALARM` | StreamLog TCP Socket 绑定失败 |
| `SKIP_READ_LOG_ALARM` | 33 | `SKIP_READ_LOG_ALARM` | 跳过日志读取 |
| `SEND_COMPRESS_FAIL_ALARM` | 34 | `SEND_COMPRESS_FAIL_ALARM` | 发送前压缩失败 |
| `PARSE_LOG_FAIL_ALARM` | 35 | `PARSE_LOG_FAIL_ALARM` | 日志解析失败 |
| `LOG_TRUNCATE_ALARM` | 36 | `LOG_TRUNCATE_ALARM` | 检测到日志文件截断 |
| `DIR_EXCEED_LIMIT_ALARM` | 37 | `DIR_EXCEED_LIMIT_ALARM` | 目录数超限 |
| `STAT_LIMIT_ALARM` | 38 | `STAT_LIMIT_ALARM` | Stat 操作达到上限 |
| `FILE_READER_EXCEED_ALARM` | 39 | `FILE_READER_EXCEED_ALARM` | 文件 Reader 数量超限 |
| `LOGTAIL_CRASH_STACK_ALARM` | 40 | `LOGTAIL_CRASH_STACK_ALARM` | Agent 崩溃堆栈信息 |
| `MODIFY_FILE_EXCEED_ALARM` | 41 | `MODIFY_FILE_EXCEED_ALARM` | 修改文件数超限 |
| `OPEN_FILE_LIMIT_ALARM` | 42 | `OPEN_FILE_LIMIT_ALARM` | 打开文件数达到上限 |
| `TOO_MANY_CONFIG_ALARM` | 43 | `TOO_MANY_CONFIG_ALARM` | 配置数量过多 |
| `SAME_CONFIG_ALARM` | 44 | `SAME_CONFIG_ALARM` | 检测到重复配置 |
| `PROCESS_QUEUE_BUSY_ALARM` | 45 | `PROCESS_QUEUE_BUSY_ALARM` | 处理队列繁忙/已满 |
| `DROP_LOG_ALARM` | 46 | `DROP_LOG_ALARM` | 日志被丢弃 |
| `CAST_SENSITIVE_WORD_ALARM` | 47 | `CAST_SENSITIVE_WORD_ALARM` | 敏感词脱敏事件 |
| `PROCESS_TOO_SLOW_ALARM` | 48 | `PROCESS_TOO_SLOW_ALARM` | 处理速度过慢 |
| `LOAD_LOCAL_EVENT_ALARM` | 49 | `LOAD_LOCAL_EVENT_ALARM` | 加载本地事件异常 |
| `WINDOWS_WORKER_START_HINTS_ALARM` | 50 | `WINDOWS_WORKER_START_HINTS_ALARM` | Windows Worker 启动提示 |
| `HOLD_ON_TOO_SLOW_ALARM` | 51 | `HOLD_ON_TOO_SLOW_ALARM` | Hold-on 处理速度过慢 |
| `INNER_PROFILE_ALARM` | 52 | `INNER_PROFILE_ALARM` | 内部 Profile 异常 |
| `FUSE_FILE_TRUNCATE_ALARM` | 53 | `FUSE_FILE_TRUNCATE_ALARM` | FUSE 文件截断 |
| `SENDING_COSTS_TOO_MUCH_TIME_ALARM` | 54 | `SENDING_COSTS_TOO_MUCH_TIME_ALARM` | 发送耗时过长 |
| `UNEXPECTED_FILE_TYPE_MODE_ALARM` | 55 | `UNEXPECTED_FILE_TYPE_MODE_ALARM` | 遇到非预期的文件类型/模式 |
| `LOG_GROUP_WAIT_TOO_LONG_ALARM` | 56 | `LOG_GROUP_WAIT_TOO_LONG_ALARM` | LogGroup 等待时间过长 |
| `CHECKPOINT_V2_ALARM` | 57 | `CHECKPOINT_V2_ALARM` | Checkpoint V2 异常 |
| `EXACTLY_ONCE_ALARM` | 58 | `EXACTLY_ONCE_ALARM` | Exactly-Once 投递异常 |
| `READ_STOPPED_CONTAINER_ALARM` | 59 | `READ_STOPPED_CONTAINER_ALARM` | 读取已停止的容器 |
| `INVALID_CONTAINER_PATH_ALARM` | 64 | `INVALID_CONTAINER_PATH_ALARM` | 无效的容器路径 |
| `COMPRESS_FAIL_ALARM` | 65 | `COMPRESS_FAIL_ALARM` | 数据压缩失败 |
| `SERIALIZE_FAIL_ALARM` | 66 | `SERIALIZE_FAIL_ALARM` | 数据序列化失败 |
| `RELABEL_METRIC_FAIL_ALARM` | 67 | `RELABEL_METRIC_FAIL_ALARM` | Prometheus 指标 Relabel 失败 |
| `REGISTER_HANDLERS_TOO_SLOW_ALARM` | 68 | `REGISTER_HANDLERS_TOO_SLOW_ALARM` | 注册文件处理器速度过慢 |
| `HOST_MONITOR_ALARM` | 69 | `HOST_MONITOR_ALARM` | 主机监控异常 |

### 5.3 Go 告警类型

以下为 Go 端 `AlarmType` 常量的完整列表，定义于 `pkg/selfmonitor/alarm_type.go`。每个常量的字符串值即为其声明的常量值。

<details>
<summary>点击展开完整 Go 告警类型列表（230+ 项）</summary>

| Go 常量 | 字符串值 | 含义 |
|---------|----------|------|
| `AggGroupAlarm` | `AGG_GROUP_ALARM` | 聚合分组异常 |
| `AggShardhashNotFoundKey` | `AGG_SHARDHASH_NOT_FOUND_KEY` | 聚合 ShardHash 未找到目标 Key |
| `AggregateOversizeAlarm` | `AGGREGATE_OVERSIZE_ALARM` | 聚合数据超过大小限制 |
| `AggregatorAddAlarm` | `AGGREGATOR_ADD_ALARM` | 向聚合器添加数据失败 |
| `AggregatorInitErrorAlarm` | `AGGREGATOR_INIT_ERROR` | 聚合器初始化错误 |
| `AnchorFindAlarm` | `ANCHOR_FIND_ALARM` | Anchor 处理器查找字段失败 |
| `AnchorJSONAlarm` | `ANCHOR_JSON_ALARM` | Anchor 处理器 JSON 解析异常 |
| `AsyncExecFailed` | `ASYNC_EXEC_FAILED` | 异步执行失败 |
| `Base64DAlarm` | `BASE64_D_ALARM` | Base64 解码异常 |
| `Base64DFindAlarm` | `BASE64_D_FIND_ALARM` | Base64 解码查找字段失败 |
| `Base64EFindAlarm` | `BASE64_E_FIND_ALARM` | Base64 编码查找字段失败 |
| `BootInitAlarm` | `BOOT_INIT_ALARM` | 启动初始化异常 |
| `BootLoadAlarm` | `BOOT_LOAD_ALARM` | 启动加载异常 |
| `BootStartAlarm` | `BOOT_START_ALARM` | 启动过程异常 |
| `BootStopAlarm` | `BOOT_STOP_ALARM` | 停止过程异常 |
| `CadvisorComposeAlarm` | `CADVISOR_COMPOSE_ALARM` | cAdvisor Compose 异常 |
| `CanalInvalidAlarm` | `CANAL_INVALID_ALARM` | Canal 输入无效配置 |
| `CanalRuntimeAlarm` | `CANAL_RUNTIME_ALARM` | Canal 运行时异常 |
| `CanalStartAlarm` | `CANAL_START_ALARM` | Canal 启动异常 |
| `CatchStandardOutputAlarm` | `CATCH_STANDARD_OUTPUT_ALARM` | 捕获标准输出异常 |
| `CategoryConfigAlarm` | `CATEGORY_CONFIG_ALARM` | 分类配置错误 |
| `CheckpointAlarm` | `CHECKPOINT_ALARM` | Checkpoint 异常 |
| `CheckpointGetAlarm` | `CHECKPOINT_GET_ALARM` | 获取 Checkpoint 失败 |
| `CheckpointInitAlarm` | `CHECKPOINT_INIT_ALARM` | Checkpoint 初始化失败 |
| `CheckpointInvalidAlarm` | `CHECKPOINT_INVALID_ALARM` | 无效的 Checkpoint |
| `CheckpointSaveAlarm` | `CHECKPOINT_SAVE_ALARM` | 保存 Checkpoint 失败 |
| `ClickhouseSubscriberAlarm` | `CLICKHOUSE_SUBSCRIBER_ALARM` | ClickHouse 订阅者异常 |
| `CloudMetaAlarm` | `CLOUD_META_ALARM` | 云元数据获取异常 |
| `CompileRegexpAlarm` | `COMPILE_REGEXP_ALARM` | 正则表达式编译失败 |
| `ConditionInitAlarm` | `CONDITION_INIT_ALARM` | 条件处理器初始化失败 |
| `ConfigLoadAlarm` | `CONFIG_LOAD_ALARM` | 配置加载异常 |
| `ConfigStopTimeoutAlarm` | `CONFIG_STOP_TIMEOUT_ALARM` | 配置停止超时 |
| `ConnectCriRuntimeAlarm` | `CONNECT_CRI_RUNTIME_ALARM` | 连接 CRI 运行时失败 |
| `ContainerCenterAlarm` | `CONTAINER_CENTER_ALARM` | 容器中心异常 |
| `ContainerdClientAlarm` | `CONTAINERD_CLIENT_ALARM` | Containerd 客户端异常 |
| `ConvertAlarm` | `CONVERT_ALARM` | 数据转换异常 |
| `CopyLogAlarm` | `COPY_LOG_ALARM` | 日志复制异常 |
| `CreateContainerdInfoAlarm` | `CREATE_CONTAINERD_INFO_ALARM` | 创建 Containerd 信息失败 |
| `DecodeBodyFailAlarm` | `DECODE_BODY_FAIL_ALARM` | 请求体解码失败 |
| `DecodeFindAlarm` | `DECODE_FIND_ALARM` | 解码查找字段失败 |
| `DecodeLogAlarm` | `DECODE_LOG_ALARM` | 日志解码异常 |
| `DecodeMessageFailAlarm` | `DECODE_MESSAGE_FAIL_ALARM` | 消息解码失败 |
| `DefaultFlusherAlarm` | `DEFAULT_FLUSHER_ALARM` | 默认 Flusher 异常 |
| `DetectContainerAlarm` | `DETECT_CONTAINER_ALARM` | 容器检测异常 |
| `DockerCenterAlarm` | `DOCKER_CENTER_ALARM` | Docker 中心异常 |
| `DockerClientAlarm` | `DOCKER_CLIENT_ALARM` | Docker 客户端异常 |
| `DockerEventAlarm` | `DOCKER_EVENT_ALARM` | Docker 事件异常 |
| `DockerExecAlarm` | `DOCKER_EXEC_ALARM` | Docker Exec 异常 |
| `DockerMatchAlarm` | `DOCKER_MATCH_ALARM` | Docker 匹配异常 |
| `DockerRegexCompileAlarm` | `DOCKER_REGEX_COMPILE_ALARM` | Docker 正则编译失败 |
| `DockerStdoutStartAlarm` | `DOCKER_STDOUT_START_ALARM` | Docker Stdout 采集启动异常 |
| `DockerStdoutStatAlarm` | `DOCKER_STDOUT_STAT_ALARM` | Docker Stdout 状态异常 |
| `DockerStdoutStopAlarm` | `DOCKER_STDOUT_STOP_ALARM` | Docker Stdout 采集停止异常 |
| `DorisSubscriberAlarm` | `DORIS_SUBSCRIBER_ALARM` | Doris 订阅者异常 |
| `DownDockerComposeError` | `DOWN_DOCKER_COMPOSE_ERROR` | Docker Compose 关闭失败 |
| `DropDataAlarm` | `DROP_DATA_ALARM` | 数据被丢弃 |
| `DumpFileAlarm` | `DUMP_FILE_ALARM` | 文件转储异常 |
| `ECSAlarm` | `ECS_ALARM` | ECS 元数据异常 |
| `ElasticsearchSubscriberAlarm` | `ELASTICSEARCH_SUBSCRIBER_ALARM` | Elasticsearch 订阅者异常 |
| `EnvFlagAlarm` | `ENV_FLAG_ALARM` | 环境变量 Flag 异常 |
| `ErrorJMXPort` | `ERROR_JMX_PORT` | JMX 端口异常 |
| `ExecAlarm` | `EXEC_ALARM` | 命令执行异常 |
| `ExportReqFailAlarm` | `EXPORT_REQ_FAIL_ALARM` | 导出请求失败 |
| `ExtensionFilterAlarm` | `EXTENSION_FILTER_ALARM` | 扩展过滤器异常 |
| `FailToInitPing` | `FAIL_TO_INIT_PING` | Ping 初始化失败 |
| `FailToRunHttping` | `FAIL_TO_RUN_HTTPING` | HTTPing 执行失败 |
| `FailToRunPing` | `FAIL_TO_RUN_PING` | Ping 执行失败 |
| `FailedCollectHostMetadata` | `FAILED_COLLECT_HOST_METADATA` | 采集主机元数据失败 |
| `FilenrPatternAlarm` | `FILENR_PATTERN_ALARM` | 文件句柄数 Pattern 异常 |
| `FilterInitAlarm` | `FILTER_INIT_ALARM` | 过滤器初始化失败 |
| `FlushDataAlarm` | `FLUSH_DATA_ALARM` | 数据刷写异常 |
| `FlusherFlushAlarm` | `FLUSHER_FLUSH_ALARM` | Flusher 刷写异常 |
| `FlusherFlushAlram` | `FLUSHER_FLUSH_ALRAM` | Flusher 刷写异常（历史拼写） |
| `FlusherInitAlarm` | `FLUSHER_INIT_ALARM` | Flusher 初始化异常 |
| `FlusherQueueFull` | `FLUSHER_QUEUE_FULL` | Flusher 队列已满 |
| `FlusherReadyAlarm` | `FLUSHER_READY_ALARM` | Flusher 就绪检查异常 |
| `FlusherStopAlarm` | `FLUSHER_STOP_ALARM` | Flusher 停止异常 |
| `GPUNVMLCollectAlarm` | `GPU_NVML_COLLECT_ALARM` | GPU NVML 指标采集异常 |
| `GPUNVMLDeviceCountAlarm` | `GPU_NVML_DEVICE_COUNT_ALARM` | GPU NVML 设备数获取异常 |
| `GPUNVMLDeviceIndexAlarm` | `GPU_NVML_DEVICE_INDEX_ALARM` | GPU NVML 设备索引异常 |
| `GPUNVMLInitAlarm` | `GPU_NVML_INIT_ALARM` | GPU NVML 初始化异常 |
| `GRPCFlushAlarm` | `GRPC_FLUSH_ALARM` | gRPC 刷写异常 |
| `GRPCFlusherAlarm` | `GRPC_FLUSHER_ALARM` | gRPC Flusher 异常 |
| `GRPCServerAlarm` | `GRPC_SERVER_ALARM` | gRPC 服务端异常 |
| `GeoipAlarm` | `GEOIP_ALARM` | GeoIP 查询异常 |
| `GetSigmaEnvError` | `GET_SIGMA_ENV_ERROR` | 获取 Sigma 环境变量失败 |
| `GoprofileAlarm` | `GOPROFILE_ALARM` | Go Profile 异常 |
| `GotimeFindAlarm` | `GOTIME_FIND_ALARM` | Go 时间格式查找失败 |
| `GotimeParseAlarm` | `GOTIME_PARSE_ALARM` | Go 时间格式解析失败 |
| `GrokFindAlarm` | `GROK_FIND_ALARM` | Grok 处理器查找字段失败 |
| `HTTPCollectAlarm` | `HTTP_COLLECT_ALARM` | HTTP 采集异常 |
| `HTTPInitAlarm` | `HTTP_INIT_ALARM` | HTTP 初始化异常 |
| `HTTPLoadAddressAlarm` | `HTTP_LOAD_ADDRESS_ALARM` | HTTP 加载地址异常 |
| `HTTPParseAlarm` | `HTTP_PARSE_ALARM` | HTTP 解析异常 |
| `HTTPServerError` | `HTTP_SERVER_ERROR` | HTTP 服务端错误 |
| `HoldonLogtailpluginAlarm` | `HOLDON_LOGTAILPLUGIN_ALARM` | 插件 Hold-on 异常 |
| `InfluxdbSubscriberAlarm` | `INFLUXDB_SUBSCRIBER_ALARM` | InfluxDB 订阅者异常 |
| `InitAlarm` | `INIT_ALARM` | 通用初始化异常 |
| `InitCatchStderrAlarm` | `INIT_CATCH_STDERR_ALARM` | 初始化 Stderr 捕获异常 |
| `InitCatchStdoutAlarm` | `INIT_CATCH_STDOUT_ALARM` | 初始化 Stdout 捕获异常 |
| `InitCheckpointAlarm` | `INIT_CHECKPOINT_ALARM` | 初始化 Checkpoint 异常 |
| `InitHTTPServerAlarm` | `INIT_HTTP_SERVER_ALARM` | 初始化 HTTP 服务异常 |
| `InitServerArmar` | `INIT_SERVER_ARMAR` | 初始化 Server Armar 异常 |
| `InputCanalAlarm` | `INPUT_CANAL_ALARM` | Canal 输入插件异常 |
| `InputCollectAlarm` | `INPUT_COLLECT_ALARM` | 输入插件采集异常 |
| `InputKafkaAlarm` | `INPUT_KAFKA_ALARM` | Kafka 输入插件异常 |
| `InputSnmpConnectionError` | `INPUT_SNMP_CONNECTION_ERROR` | SNMP 输入连接错误 |
| `InvalidAggregatorTypeAlarm` | `INVALID_AGGREGATOR_TYPE` | 无效的聚合器类型 |
| `InvalidKafkaPartitioner` | `INVALID_KAFKA_PARTITIONER` | 无效的 Kafka 分区器 |
| `InvalidProcessorTypeAlarm` | `INVALID_PROCESSOR_TYPE` | 无效的处理器类型 |
| `InvalidRegexAlarm` | `INVALID_REGEX_ALARM` | 无效的正则表达式 |
| `JmxfetchAlarm` | `JMXFETCH_ALARM` | JMXFetch 异常 |
| `JmxfetchRuntimeAlarm` | `JMXFETCH_RUNTIME_ALARM` | JMXFetch 运行时异常 |
| `JournalReadAlarm` | `JOURNAL_READ_ALARM` | Systemd Journal 读取异常 |
| `JournalSeekAlarm` | `JOURNAL_SEEK_ALARM` | Systemd Journal Seek 异常 |
| `K8sMetaCollectorError` | `K8S_META_COLLECTOR_ERROR` | K8s 元信息采集器错误 |
| `KafkaSubscriberAlarm` | `KAFKA_SUBSCRIBER_ALARM` | Kafka 订阅者异常 |
| `KubePrometheusAlarm` | `KUBE_PROMETHEUS_ALARM` | Kube Prometheus 异常 |
| `KubernetesLabelSelectorError` | `KUBERNETES_LABEL_SELECTOR_ERROR` | K8s Label Selector 错误 |
| `KubernetesMetaAlarm` | `KUBERNETES_META_ALARM` | K8s 元数据异常 |
| `KubernetesMetaCollectError` | `KUBERNETES_META_COLLECT_ERROR` | K8s 元数据采集错误 |
| `KubernetesMetaFetchIntervalAlarm` | `KUBERNETES_META_FETCH_INTERVAL_ALARM` | K8s 元数据拉取间隔异常 |
| `KvSplitterAlarm` | `KV_SPLITTER_ALARM` | KV 分割器异常 |
| `ListHistoryDumpAlarm` | `LIST_HISTORY_DUMP_ALARM` | 历史列表转储异常 |
| `LoadConfigAlarm` | `LOAD_CONFIG_ALARM` | 加载配置异常 |
| `LoadPluginAlarm` | `LOAD_PLUGIN_ALARM` | 加载插件异常 |
| `LogRegexFindAlarm` | `LOG_REGEX_FIND_ALARM` | 日志正则查找失败 |
| `LoongcollectorComposeAlarm` | `LOONGCOLLECTOR_COMPOSE_ALARM` | LoongCollector Compose 异常 |
| `LumberConnectionAlarm` | `LUMBER_CONNECTION_ALARM` | Lumberjack 连接异常 |
| `LumberListenAlarm` | `LUMBER_LISTEN_ALARM` | Lumberjack 监听异常 |
| `MD5FindAlarm` | `MD5_FIND_ALARM` | MD5 处理器查找字段失败 |
| `MarshalRespFailAlarm` | `MARSHAL_RESP_FAIL_ALARM` | 响应序列化失败 |
| `MetricInputV2StartFailureAlarm` | `METRIC_INPUT_V2_START_FAILURE` | Metric Input V2 启动失败 |
| `MonitorStartAlarm` | `MONITOR_START_ALARM` | 监控启动异常 |
| `MonitorTimeoutAlarm` | `MONITOR_TIMEOUT_ALARM` | 监控超时 |
| `MqttConnectAlarm` | `MQTT_CONNECT_ALARM` | MQTT 连接异常 |
| `MqttConnectionLostAlarm` | `MQTT_CONNECTION_LOST_ALARM` | MQTT 连接丢失 |
| `MqttSubscribeAlarm` | `MQTT_SUBSCRIBE_ALARM` | MQTT 订阅异常 |
| `MultiHTTPServerAlarm` | `MULTI_HTTP_SERVER_ALARM` | 多 HTTP 服务异常 |
| `MysqlCheckpoingAlarm` | `MYSQL_CHECKPOING_ALARM` | MySQL Checkpoint 异常（历史拼写） |
| `MysqlCheckpointAlarm` | `MYSQL_CHECKPOINT_ALARM` | MySQL Checkpoint 异常 |
| `MysqlInitAlarm` | `MYSQL_INIT_ALARM` | MySQL 初始化异常 |
| `MysqlParseAlarm` | `MYSQL_PARSE_ALARM` | MySQL 解析异常 |
| `MysqlQueryAlarm` | `MYSQL_QUERY_ALARM` | MySQL 查询异常 |
| `MysqlTimeoutAlarm` | `MYSQL_TIMEOUT_ALARM` | MySQL 超时 |
| `NginxStatusCollectAlarm` | `NGINX_STATUS_COLLECT_ALARM` | Nginx Status 采集异常 |
| `NginxStatusInitAlarm` | `NGINX_STATUS_INIT_ALARM` | Nginx Status 初始化异常 |
| `NoMatchRouterAlarm` | `NO_MATCH_ROUTER_ALARM` | 无匹配路由 |
| `Open1mountsAlarm` | `OPEN_1MOUNTS_ALARM` | 打开 /proc/1/mounts 异常 |
| `OpenFilenrAlarm` | `OPEN_FILENR_ALARM` | 打开 file-nr 异常 |
| `OpenProcfsAlarm` | `OPEN_PROCFS_ALARM` | 打开 procfs 异常 |
| `PackJSONAlarm` | `PACK_JSON_ALARM` | JSON 打包异常 |
| `ParseDockerLineAlarm` | `PARSE_DOCKER_LINE_ALARM` | Docker 日志行解析异常 |
| `PathAlarm` | `PATH_ALARM` | 路径异常 |
| `PluginAlarm` | `PLUGIN_ALARM` | 通用插件异常 |
| `PluginRunAlarm` | `PLUGIN_RUN_ALARM` | 插件运行异常 |
| `PluginRuntimeAlarm` | `PLUGIN_RUNTIME_ALARM` | 插件运行时异常 |
| `PluginUnmarshalAlarm` | `PLUGIN_UNMARSHAL_ALARM` | 插件配置反序列化异常 |
| `PprofProfileAlarm` | `PPROF_PROFILE_ALARM` | pprof Profile 异常 |
| `ProcessLabelTooLongAlarm` | `PROCESS_LABEL_TOO_LONG_ALARM` | 进程 Label 超长 |
| `ProcessListAlarm` | `PROCESS_LIST_ALARM` | 进程列表异常 |
| `ProcessorDesensitizeAlarm` | `PROCESSOR_DESENSITIZE_ALARM` | 脱敏处理器异常 |
| `ProcessorEncryptAlarm` | `PROCESSOR_ENCRYPT_ALARM` | 加密处理器异常 |
| `ProcessorInitAlarm` | `PROCESSOR_INIT_ALARM` | 处理器初始化异常 |
| `ProcessorJSONFindAlarm` | `PROCESSOR_JSON_FIND_ALARM` | JSON 处理器查找字段失败 |
| `ProcessorJSONParserAlarm` | `PROCESSOR_JSON_PARSER_ALARM` | JSON 处理器解析异常 |
| `ProcessorOTELMetricDataFormat` | `PROCESSOR_OTEL_METRIC_DATA_FORMAT` | OTEL Metric 数据格式异常 |
| `ProcessorOTELMetricFindAlarm` | `PROCESSOR_OTEL_METRIC_FIND_ALARM` | OTEL Metric 查找异常 |
| `ProcessorOTELTraceDataFormat` | `PROCESSOR_OTEL_TRACE_DATA_FORMAT` | OTEL Trace 数据格式异常 |
| `ProcessorOTELTraceFindAlarm` | `PROCESSOR_OTEL_TRACE_FIND_ALARM` | OTEL Trace 查找异常 |
| `ProcessorOTELTraceParserAlarm` | `PROCESSOR_OTEL_TRACE_PARSER_ALARM` | OTEL Trace 解析异常 |
| `ProcessorSplitLogStringFindAlarm` | `PROCESSOR_SPLIT_LOG_STRING_FIND_ALARM` | 日志分割处理器查找失败 |
| `ProcessorStringReplaceAlarm` | `PROCESSOR_STRING_REPLACE_ALARM` | 字符串替换处理器异常 |
| `PrometheusFlusherInitAlarm` | `PROMETHEUS_FLUSHER_INIT_ALARM` | Prometheus Flusher 初始化异常 |
| `PyroscopeAlarm` | `PYROSCOPE_ALARM` | Pyroscope 异常 |
| `Read1mountsAlarm` | `READ_1MOUNTS_ALARM` | 读取 /proc/1/mounts 异常 |
| `ReadBodyFailAlarm` | `READ_BODY_FAIL_ALARM` | 读取请求体失败 |
| `ReadDiskAlarm` | `READ_DISK_ALARM` | 读取磁盘异常 |
| `ReadFileAlarm` | `READ_FILE_ALARM` | 读取文件异常 |
| `ReadFilenrAlarm` | `READ_FILENR_ALARM` | 读取 file-nr 异常 |
| `ReadProcAlarm` | `READ_PROC_ALARM` | 读取 /proc 异常 |
| `ReadPtoctcp6Alarm` | `READ_PTOCTCP6_ALARM` | 读取 /proc/net/tcp6 异常 |
| `ReadPtoctcpAlarm` | `READ_PTOCTCP_ALARM` | 读取 /proc/net/tcp 异常 |
| `ReadStaticConfigAlarm` | `READ_STATIC_CONFIG_ALARM` | 读取静态配置异常 |
| `ReceiveLogGroupAlarm` | `RECEIVE_LOG_GROUP_ALARM` | 接收 LogGroup 异常 |
| `ReceiveRawLogAlarm` | `RECEIVE_RAW_LOG_ALARM` | 接收原始日志异常 |
| `RedisCollectAlarm` | `REDIS_COLLECT_ALARM` | Redis 采集异常 |
| `RedisParseAddressAlarm` | `REDIS_PARSE_ADDRESS_ALARM` | Redis 地址解析异常 |
| `RegexCompileAlarm` | `REGEX_COMPILE_ALARM` | 正则表达式编译失败 |
| `RegexFindAlarm` | `REGEX_FIND_ALARM` | 正则查找失败 |
| `RegexUnmatchedAlarm` | `REGEX_UNMATCHED_ALARM` | 正则未匹配 |
| `RenameFindAlarm` | `RENAME_FIND_ALARM` | 重命名处理器查找字段失败 |
| `RuntimeAlarm` | `RUNTIME_ALARM` | 运行时异常 |
| `SaveCheckpointAlarm` | `SAVE_CHECKPOINT_ALARM` | 保存 Checkpoint 异常 |
| `ServiceOTLPInvalidGRPCServerConfig` | `SERVICE_OTLP_INVALID_GRPC_SERVER_CONFIG` | OTLP gRPC 服务配置无效 |
| `ServiceSyslogCloseAlarm` | `SERVICE_SYSLOG_CLOSE_ALARM` | Syslog 服务关闭异常 |
| `ServiceSyslogInitAlarm` | `SERVICE_SYSLOG_INIT_ALARM` | Syslog 服务初始化异常 |
| `ServiceSyslogPacketAlarm` | `SERVICE_SYSLOG_PACKET_ALARM` | Syslog 数据包异常 |
| `ServiceSyslogParseAlarm` | `SERVICE_SYSLOG_PARSE_ALARM` | Syslog 解析异常 |
| `ServiceSyslogStreamAlarm` | `SERVICE_SYSLOG_STREAM_ALARM` | Syslog 流异常 |
| `ServiceTelegrafOverwriteConfigAlarm` | `SERVICE_TELEGRAF_OVERWRITE_CONFIG_ALARM` | Telegraf 配置覆写异常 |
| `ServiceTelegrafRemoveConfigAlarm` | `SERVICE_TELEGRAF_REMOVE_CONFIG_ALARM` | Telegraf 配置移除异常 |
| `ServiceTelegrafRuntimeAlarm` | `SERVICE_TELEGRAF_RUNTIME_ALARM` | Telegraf 运行时异常 |
| `SkywalkingCollectTraceError` | `SKYWALKING_COLLECT_TRACE_ERROR` | SkyWalking Trace 采集错误 |
| `SkywalkingLoadCheckpointFail` | `SKYWALKING_LOAD_CHECKPOINT_FAIL` | SkyWalking 加载 Checkpoint 失败 |
| `SkywalkingMeterGRPCError` | `SKYWALKING_METER_GRPC_ERROR` | SkyWalking Meter gRPC 错误 |
| `SkywalkingResourceNotReady` | `SKYWALKING_RESOURCE_NOT_READY` | SkyWalking 资源未就绪 |
| `SkywalkingSaveCheckpointFail` | `SKYWALKING_SAVE_CHECKPOINT_FAIL` | SkyWalking 保存 Checkpoint 失败 |
| `SkywalkingToOtTraceErr` | `SKYWALKING_TO_OT_TRACE_ERR` | SkyWalking 转换为 OT Trace 错误 |
| `SkywalkingTopicNotRecognized` | `SKYWALKING_TOPIC_NOT_RECOGNIZED` | SkyWalking Topic 无法识别 |
| `SplitFindAlarm` | `SPLIT_FIND_ALARM` | 分割处理器查找字段失败 |
| `SplitLogAlarm` | `SPLIT_LOG_ALARM` | 日志分割异常 |
| `StartDockerComposeError` | `START_DOCKER_COMPOSE_ERROR` | Docker Compose 启动失败 |
| `StartPluginAlarm` | `START_PLUGIN_ALARM` | 插件启动异常 |
| `StatFileAlarm` | `STAT_FILE_ALARM` | 文件 Stat 异常 |
| `StatsdParseAlarm` | `STATSD_PARSE_ALARM` | StatsD 解析异常 |
| `StderrAlarm` | `STDERR_ALARM` | Stderr 输出告警 |
| `StopDockerComposeError` | `STOP_DOCKER_COMPOSE_ERROR` | Docker Compose 停止失败 |
| `StopExtensionAlarm` | `STOP_EXTENSION_ALARM` | 扩展停止异常 |
| `StopFlusherAlarm` | `STOP_FLUSHER_ALARM` | Flusher 停止异常 |
| `StrptimeParseAlarm` | `STRPTIME_PARSE_ALARM` | Strptime 时间解析异常 |
| `SyncContainerdAlarm` | `SYNC_CONTAINERD_ALARM` | 同步 Containerd 信息异常 |
| `TaskExecuteSlowAlarm` | `TASK_EXECUTE_SLOW` | 任务执行过慢 |
| `TelegrafAlarm` | `TELEGRAF_ALARM` | Telegraf 异常 |
| `UDPServerAlarm` | `UDP_SERVER_ALARM` | UDP 服务异常 |
| `WineventlogApiAlarm` | `WINEVENTLOG_API_ALARM` | Windows EventLog API 异常 |
| `WineventlogMainAlarm` | `WINEVENTLOG_MAIN_ALARM` | Windows EventLog 主流程异常 |
| `WineventlogUtilAlarm` | `WINEVENTLOG_UTIL_ALARM` | Windows EventLog 工具异常 |
| `WrongProtobufAlarm` | `WRONG_PROTOBUF_ALARM` | Protobuf 格式错误 |

</details>

### 5.4 C++/Go 共有的告警类型

以下告警类型在 C++ 和 Go 两端都有定义（部分字符串值相同，部分不同）：

| C++ 枚举 | Go 常量 | C++ 字符串值 | Go 字符串值 | 含义 | 备注 |
|----------|---------|-------------|------------|------|------|
| `CATEGORY_CONFIG_ALARM` | `CategoryConfigAlarm` | `CATEGORY_CONFIG_ALARM` | `CATEGORY_CONFIG_ALARM` | 分类配置错误 | 两端一致 |
| `CHECKPOINT_ALARM` | `CheckpointAlarm` | `CHECKPOINT_ALARM` | `CHECKPOINT_ALARM` | Checkpoint 异常 | 两端一致 |
| `DROP_LOG_ALARM` | `DropDataAlarm` | `DROP_LOG_ALARM` | `DROP_DATA_ALARM` | 数据被丢弃 | 两端字符串不同 |
