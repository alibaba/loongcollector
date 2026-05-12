---
name: selfmonitor
description: Self-monitoring metrics, alarm code standards for LoongCollector. Read when changes involve metrics, alarms, or observability.
---
# Self-Monitoring Code Standards

You are a self-monitoring code quality expert, responsible for ensuring LoongCollector code correctly uses self-monitoring features including metrics, alarms, code style, and implementation logic.

## Metric Naming Conventions

### Format

**Variable name**: `{MODULE}_{METRIC_CONTENT_DESCRIPTION}_{UNIT}` (ALL CAPS)
**Variable content**: `{metric_content_description}_{unit}` (all lowercase)

Example:
```cpp
const string METRIC_RUNNER_FLUSHER_IN_RAW_SIZE_BYTES = "in_raw_size_bytes";
```

### Module Prefix Categories

- **`agent_`**: Process-level metrics, describing entire Agent state
- **`pipeline_`**: Pipeline-level metrics, describing data pipeline state
- **`plugin_`**: Plugin-level metrics, describing specific plugin state
- **`component_`**: Component-level metrics, describing internal component state
- **`runner_`**: Runner-level metrics, describing runner state

### Unit Categories

#### Counter metrics
- **`_total`**: Cumulative count (default), e.g. `input_records_total`, `send_success_total`

#### Size metrics
- **`_bytes`**: Bytes, e.g. `input_size_bytes`, `memory_used_bytes`
- **`_mb`**: Megabytes (memory), e.g. `agent_memory_used_mb`

#### Time metrics
- **`_ms`**: Milliseconds (processing time, latency), e.g. `process_time_ms`
- **`_s`**: Seconds (long intervals), e.g. `uptime_s`

#### Ratio metrics
- **`_percent`**: Percentage, e.g. `cpu_usage_percent`
- **`_ps`**: Per second (rate), e.g. `send_bytes_ps`

#### State metrics
- **`_flag`**: Flag (0 or 1), e.g. `enabled_flag`
- **`_state`**: State value, e.g. `register_state`

### Label Naming Conventions

**Label Key format**: `METRIC_LABEL_KEY_{description}`

Common keys: `METRIC_LABEL_KEY_PROJECT`, `METRIC_LABEL_KEY_LOGSTORE`, `METRIC_LABEL_KEY_PIPELINE_NAME`, `METRIC_LABEL_KEY_PLUGIN_TYPE`, `METRIC_LABEL_KEY_PLUGIN_ID`, `METRIC_LABEL_KEY_FILE_NAME`, `METRIC_LABEL_KEY_FILE_DEV`, `METRIC_LABEL_KEY_FILE_INODE`, `METRIC_LABEL_KEY_REGION`, `METRIC_LABEL_KEY_RUNNER_NAME`

## Alarm Level Conventions

Based on PR #2319 design, alarm levels:

| Level | Severity | Description | Typical Scenario |
|-------|----------|-------------|------------------|
| 1 | warning | Single point error, doesn't affect overall flow | Data parse failure; single collection/send failure |
| 2 | error | Affects main flow, risk if not optimized | Queue busy; monitor exceeded; unsuccessful init |
| 3 | critical | Severe impact: config/module unusable; affects agent stability; causes customer loss | Config load failure; unsuccessful module init; data drop; crash |

### C++ Alarm Usage

**Correct**:
```cpp
AlarmManager::GetInstance()->SendAlarmWarning(LOGTAIL_CONFIG_ALARM, "配置解析失败");
AlarmManager::GetInstance()->SendAlarmError(PROCESS_QUEUE_BUSY_ALARM, "处理队列繁忙");
AlarmManager::GetInstance()->SendAlarmCritical(CATEGORY_CONFIG_ALARM, "配置加载失败");
```

**Wrong**: Don't use old `SendAlarm` interface.

### Go Alarm Usage

**Correct**:
```go
logger.Warning(ctx, selfmonitor.CategoryConfigAlarm, "配置解析失败")
logger.Error(ctx, selfmonitor.ProcessQueueBusyAlarm, "处理队列繁忙")
logger.Critical(ctx, selfmonitor.CategoryConfigAlarm, "配置加载失败")
```

## Adding New Metrics

### C++ Steps

1. **Define metric constants**: Add to `core/monitor/metric_constants/MetricConstants.h`
2. **Create MetricsRecordRef** with labels in Init()
3. **Create metric objects** (CounterPtr, IntGaugePtr) BEFORE commit
4. **Update values** using macros: `ADD_COUNTER()`, `SET_GAUGE()`, `ADD_GAUGE()`

**Critical**: MetricsRecordRef must create all metric objects BEFORE commit. After commit, no new metrics can be created. Use `IsCommitted()` to check state. If a Gauge default is non-zero, set it once during Init.

### Go Steps

1. **Define constants**: Add to `pkg/selfmonitor/metrics_constants_*.go`
2. **Register metrics** in `InitMetricRecord()`:
   ```go
   p.MetricRecord = p.Config.Context.RegisterMetricRecord(labels)
   p.metricCounter = selfmonitor.NewCounterMetricAndRegister(p.MetricRecord, selfmonitor.MetricPluginInEventsTotal)
   ```
3. **Update values**: Check nil before updating.

## Adding New Alarm Types

### C++ Steps

1. Add to `core/monitor/AlarmManager.h` enum `AlarmType`
2. Add to `mMessageType` vector in `AlarmManager.cpp` constructor
3. Use leveled interfaces: `SendAlarmWarning`, `SendAlarmError`, `SendAlarmCritical`

### Go Steps

1. Add to `pkg/selfmonitor/alarm_constants.go`
2. Use leveled interfaces: `logger.Warning`, `logger.Error`, `logger.Critical`

## Best Practices

1. Create metric objects once during initialization, not per-call
2. Use safe update macros that check for null
3. Choose alarm level matching severity
4. Provide meaningful alarm messages with context
5. Avoid alarm storms - limit frequency of same alarm
6. Metrics should not impact main flow performance

## Checklist

Before submitting self-monitoring code:
- [ ] Metric names follow naming convention with correct module prefix and unit
- [ ] Labels follow naming convention
- [ ] Correct alarm level interface used, matching severity
- [ ] No deprecated interfaces used
- [ ] Metrics created once, updated safely
- [ ] Alarm storms avoided
- [ ] Error handling complete
