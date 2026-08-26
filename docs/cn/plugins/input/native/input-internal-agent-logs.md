# 本机诊断日志（一次性）

## 简介

`input_internal_agent_logs_onetime` 采集 LoongCollector 本机运行日志以及文本态运行文件 / 配置，并以 [LogEvent](../../../developer-guide/data-model-cpp.md) 的格式暴露出去。路径由本机运行目录决定，配置里不填 `FilePaths`。

本插件是 **onetime** 输入，**不是**全局 singleton：不同时间窗口的诊断任务可以同时下发、并行采集。加载时会在内部展开多组 `input_static_file_onetime`（运行日志解析、整文件配置、checkpoint）。EventGroup 会带 metadata `INTERNAL_DATA_TYPE=__agent_log__`，普通 flusher 可忽略该字段。一条 pipeline 的 JSON 里只写这一个 input，不要再并列其他 input。

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| ✓ | — | — |

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.1.9 及以上

## 配置参数

| 参数 | 类型 | 是否必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| Type | string | 是 | / | 固定为 `input_internal_agent_logs_onetime`。 |
| Aliuid | string | 否 | 空 | 写入 `__tag__:aliuid`，便于检索。开源场景一般不填。 |
| IPList | []string | 否 | 空 | 本机 IP 精确匹配（与心跳 IP 同源）才采集。缺省或空表示不过滤。未命中时 Init 成功但不读文件。 |
| StartTime | int | 否 | / | Unix 秒。与 EndTime 同时有效且 `StartTime < EndTime` 时，只过滤**运行日志行**。 |
| EndTime | int | 否 | / | Unix 秒。闭区间上界。配置 / checkpoint 等整文件**不过时间过滤**。 |

该插件必须配合 `global.ExcutionTimeout` 使用（一次性配置）。建议同时设置 `ForceRerunWhenUpdate: true`。不要填写 `FilePaths`、`processors`。

## 采集范围

路径运行时由 `GetAgentLogDir` / `GetAgentRunDir` / `GetAgentDataDir` / `GetLoongcollectorConfDir` 等决定，兼容商业版、集团包与 `logtail_mode`。缺文件跳过，不告警。

### 运行日志（按行解析 + 可选时间过滤）

- C++：`{log}/loongcollector.LOG*` 或 `{log}/ilogtail.LOG*`
- Go：`{log}/go_plugin.LOG*` 或 `{log}/logtail_plugin.LOG*`
- 包含未压缩轮转本 `.LOG.1`、`.LOG.2` 等
- 已压成 `.gz` 的**不采**，对每个文件发 `SKIP_READ_LOG_ALARM`，文案含绝对路径和最后修改时间

### 整文件（一条事件 = 一份文件，默认上限 10MB）

状态与小文件：`app_info.json`、`inotify_watcher_dirs`、主配置、`docker_path_config.json`、`onetime_config_info.json`、`apsara_log_conf.json`、`plugin_logger.xml`、`user_defined_id`、`logger_initialization.log`、`self_metrics.log`、`backtrace.dat`，以及环境变量 `ALIYUN_LOG_STATIC_CONTAINER_INFO` 指向的文件（有才采）。

目录 glob：pipeline / instance / `config.d` / legacy / `input_static_file` 下的文本配置。

`file_check_point`（或 logtail_mode 下的 `logtail_check_point`）整文件采集，超限按现有 `whole_file` 截断。

不采集 LevelDB checkpoint、发送缓存、eBPF `.so`、Windows dmp。

每条事件打 tag `artifact`（如 `cpp_log` / `app_info` / `file_checkpoint` / `pipeline_config`），可选 `aliuid`。

## 样例

采集本机诊断文件并写到本地：

```yaml
enable: true
global:
  ExcutionTimeout: 3600
  ForceRerunWhenUpdate: true
inputs:
  - Type: input_internal_agent_logs_onetime
    IPList:
      - 10.0.1.11
      - 10.0.1.12
    StartTime: 1756032000
    EndTime: 1756035600
flushers:
  - Type: flusher_file
    FilePath: self_monitor/agent_logs.log
```
