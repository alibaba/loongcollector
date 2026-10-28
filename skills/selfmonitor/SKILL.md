---
name: selfmonitor
description: 自监控指标、告警代码标准
---
# 自监控代码标准

你是一个自监控代码质量专家，负责确保LoongCollector中所有代码正确使用自监控功能。你的职责是检查指标使用、告警使用、代码风格和实现逻辑的正确性。

## 指标命名规范

### 命名格式

**变量名基本格式**: `{模块}_{指标内容描述}_{单位}`（全部大写）

**变量内容基本格式**: `{指标内容描述}_{单位}`（全部小写）

例如：

```cpp
const string METRIC_RUNNER_FLUSHER_IN_RAW_SIZE_BYTES = "in_raw_size_bytes";
```

### 模块前缀分类

- **`agent_`**: 进程级指标，描述整个Agent的状态
- **`pipeline_`**: Pipeline级指标，描述数据流水线的状态
- **`plugin_`**: 插件级指标，描述具体插件的状态
- **`component_`**: 组件级指标，描述内部组件的状态
- **`runner_`**: Runner级指标，描述运行器的状态

### 单位分类规范

根据指标类型和用途，使用以下标准单位：

#### 计数类指标

- **`_total`**: 累计总数（默认单位，无单位时使用）
  - 示例: `input_records_total`, `send_success_total`, `error_count_total`

#### 大小类指标

- **`_bytes`**: 字节数
  - 示例: `input_size_bytes`, `memory_used_bytes`, `file_size_bytes`
- **`_mb`**: 兆字节（内存使用等）
  - 示例: `agent_memory_used_mb`, `go_memory_used_mb`

#### 时间类指标

- **`_ms`**: 毫秒（处理时间、延迟等）
  - 示例: `process_time_ms`, `send_delay_ms`, `read_delay_ms`
- **`_s`**: 秒（长时间间隔）
  - 示例: `uptime_s`, `last_run_time_s`

#### 比率类指标

- **`_percent`**: 百分比
  - 示例: `cpu_usage_percent`, `memory_usage_percent`
- **`_ps`**: 每秒（速率）
  - 示例: `send_bytes_ps`, `process_lines_ps`

#### 状态类指标

- **`_flag`**: 标志位（0或1）
  - 示例: `enabled_flag`, `valid_flag`
- **`_state`**: 状态值
  - 示例: `register_state`, `connection_state`

### 指标内容描述规范

- 使用下划线分隔的英文描述
- 动词使用过去分词形式：`processed`, `sent`, `failed`
- 名词使用复数形式：`events`, `records`, `errors`
- 避免缩写，使用完整单词

**正确示例**:

```cpp
METRIC_AGENT_CPU_PERCENT
METRIC_AGENT_MEMORY_USED_MB
METRIC_PLUGIN_IN_EVENTS_TOTAL
METRIC_PLUGIN_OUT_SIZE_BYTES
METRIC_PIPELINE_PROCESSORS_TOTAL_PROCESS_TIME_MS
METRIC_RUNNER_LAST_RUN_TIME
METRIC_COMPONENT_QUEUE_SIZE
```

**错误示例**:

```cpp
// 缺少模块前缀
CPU_PERCENT
// 单位不规范
METRIC_AGENT_MEMORY_USED_KB
// 命名不清晰
METRIC_PLUGIN_DATA
// 缩写不规范
METRIC_AGENT_MEM_MB
```

### Label命名规范

**Label Key格式**: `METRIC_LABEL_KEY_{描述}`

**常用Label Key**:

```cpp
METRIC_LABEL_KEY_PROJECT        // 项目名
METRIC_LABEL_KEY_LOGSTORE       // 日志库名
METRIC_LABEL_KEY_PIPELINE_NAME  // 流水线名
METRIC_LABEL_KEY_PLUGIN_TYPE    // 插件类型
METRIC_LABEL_KEY_PLUGIN_ID      // 插件ID
METRIC_LABEL_KEY_FILE_NAME      // 文件名
METRIC_LABEL_KEY_FILE_DEV       // 设备号
METRIC_LABEL_KEY_FILE_INODE     // inode号
METRIC_LABEL_KEY_REGION         // 地域
METRIC_LABEL_KEY_RUNNER_NAME    // 运行器名
```

## 告警等级使用规范

### 告警等级定义

根据PR #2319的设计，告警等级分为3个级别：

| 等级 | 严重程度 | 说明 | 典型场景 |
|------|----------|------|----------|
| 1 | warning | 单点报错，不影响整体流程 | 数据解析失败；单次采集/发送失败 |
| 2 | error | 对主要流程有影响，如果不优化处理可能导致风险 | 队列繁忙；监控超限；未成功的初始化操作 |
| 3 | critical | 严重影响，会导致：采集配置/重要模块不可用；对Agent稳定性造成影响；会导致客户资损 | 配置加载失败；未成功的模块初始化操作；丢弃数据；crash |

### C++告警使用规范

**正确使用方式**:

```cpp
// 使用新的等级化接口
AlarmManager::GetInstance()->SendAlarmWarning(LOGTAIL_CONFIG_ALARM, "配置解析失败");
AlarmManager::GetInstance()->SendAlarmError(PROCESS_QUEUE_BUSY_ALARM, "处理队列繁忙");
AlarmManager::GetInstance()->SendAlarmCritical(CATEGORY_CONFIG_ALARM, "配置加载失败");
```

**错误使用方式**:

```cpp
// 不要使用旧的SendAlarm接口
AlarmManager::GetInstance()->SendAlarm(LOGTAIL_CONFIG_ALARM, "配置解析失败");
```

### Go告警使用规范

**正确使用方式**:

```go
// 使用等级化接口
logger.Warning(ctx, selfmonitor.CategoryConfigAlarm, "配置解析失败")
logger.Error(ctx, selfmonitor.ProcessQueueBusyAlarm, "处理队列繁忙") 
logger.Critical(ctx, selfmonitor.CategoryConfigAlarm, "配置加载失败")
```

**错误使用方式**:

```go
// 不要使用未定义的告警类型
logger.Warning(ctx, "UNKNOWN_ALARM", "未知告警")
```

## 新增指标操作指南

### C++新增指标步骤

1. **定义指标常量**:
   - 在 `core/monitor/metric_constants/MetricConstants.h` 中添加指标常量
   - 在对应的 `.cpp` 文件中实现常量值

2. **创建MetricsRecordRef**:

    ```cpp
    // 在类中定义MetricsRecordRef
    MetricsRecordRef mMetricsRecordRef;

    // 在初始化函数中创建MetricsRecordRef
    void Plugin::Init() {
        // 准备labels
        MetricLabelsPtr labels = std::make_shared<MetricLabels>();
        labels->emplace_back(METRIC_LABEL_KEY_PROJECT, mProject);
        labels->emplace_back(METRIC_LABEL_KEY_PLUGIN_TYPE, mPluginType);
        
        // 创建MetricsRecordRef
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
            mMetricsRecordRef, 
            std::move(labels), 
            nullptr  // dynamicLabels
        );
    }
    ```

3. **创建指标对象**:

    ```cpp
    // 在类中定义指标指针
    CounterPtr mCounterPtr;
    IntGaugePtr mGaugePtr;

    // 在MetricsRecordRef创建后，commit前创建指标对象
    void Plugin::Init() {
        // ... 创建MetricsRecordRef的代码 ...
        
        // 创建指标对象（必须在commit前）
        mCounterPtr = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_IN_EVENTS_TOTAL);
        mGaugePtr = mMetricsRecordRef.CreateIntGauge(METRIC_PLUGIN_QUEUE_SIZE);
        
        // 提交MetricsRecordRef，之后不能再创建新指标
        mMetricsRecordRef.Commit();
    }
    ```

4. **更新指标值**:

    ```cpp
    // 使用宏进行安全更新
    void Plugin::ProcessData() {
        // 检查MetricsRecordRef是否已提交
        if (mMetricsRecordRef.IsCommitted()) {
            ADD_COUNTER(mCounterPtr, value);
            SET_GAUGE(mGaugePtr, value);
            ADD_GAUGE(mGaugePtr, delta);
        }
    }
    ```

**重要注意事项**:

- `MetricsRecordRef`必须在commit前创建所有指标对象
- commit后不能再调用`CreateCounter`、`CreateIntGauge`等方法
- 使用`IsCommitted()`检查状态，避免在已提交后创建指标
- 若某个Gauge类型的指标对应的参数的默认值非0，需要在Init的时候先将值Set一次

### Go新增指标步骤

1. **定义指标常量**:
   - 在 `pkg/selfmonitor/metrics_constants_*.go` 中添加指标常量

    ```go
    // 在 metrics_constants_plugin.go 中添加
    const (
        MetricPluginInEventsTotal = "in_events_total"
        MetricPluginQueueSize     = "queue_size"
    )
    ```

2. **注册指标**:

    ```go
    // 在插件结构体中定义指标
    type Plugin struct {
        pipeline.PluginContext
        metricCounter selfmonitor.CounterMetric
        metricGauge   selfmonitor.GaugeMetric
        // ... 其他字段
    }

    // 在插件初始化时注册指标
    func (p *Plugin) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
        // 获取插件通用labels
        labels := pipeline.GetPluginCommonLabels(p.Config.Context, pluginMeta)
        
        // 注册MetricsRecord
        p.MetricRecord = p.Config.Context.RegisterMetricRecord(labels)
        
        // 创建并注册指标对象
        p.metricCounter = selfmonitor.NewCounterMetricAndRegister(p.MetricRecord, selfmonitor.MetricPluginInEventsTotal)
        p.metricGauge = selfmonitor.NewGaugeMetricAndRegister(p.MetricRecord, selfmonitor.MetricPluginQueueSize)
    }
    ```

3. **更新指标值**:

    ```go
    // 更新指标值
    func (p *Plugin) ProcessData() {
        // 安全更新指标值
        if p.metricCounter != nil {
            p.metricCounter.Add(1)
        }
        if p.metricGauge != nil {
            p.metricGauge.Set(queueSize)
        }
    }
    ```

**重要注意事项**:

- 使用`InitMetricRecord`方法初始化指标
- 通过`NewCounterMetricAndRegister`和`NewGaugeMetricAndRegister`创建并注册指标
- 更新指标值前检查对象是否为nil
- 指标会自动上报，无需手动提交

## 新增告警类型操作指南

### C++新增告警类型步骤

1. **在 `core/monitor/AlarmManager.h` 中添加告警类型**:

    ```cpp
    enum AlarmType {
        // ... 现有类型
        NEW_ALARM_TYPE = 70,  // 使用下一个可用数字
        ALL_LOGTAIL_ALARM_NUM = 71  // 更新总数
    };
    ```

2. **在告警消息类型映射中添加**:

    ```cpp
    // 在 AlarmManager.cpp 的构造函数中添加
    AlarmManager::AlarmManager() {
        // ... 现有代码 ...
        mMessageType.push_back("NEW_ALARM_TYPE");  // 添加新告警类型
    }
    ```

3. **使用告警**:

    ```cpp
    // 在需要发送告警的地方
    void SomeFunction() {
        // 使用等级化接口发送告警
        AlarmManager::GetInstance()->SendAlarmWarning(
            NEW_ALARM_TYPE, 
            "具体错误信息: " + errorDetails
        );
        
        // 或者使用其他等级
        AlarmManager::GetInstance()->SendAlarmError(
            NEW_ALARM_TYPE, 
            "严重错误信息"
        );
    }
    ```

**重要注意事项**:

- 告警类型枚举值必须连续，不能跳跃
- 更新`ALL_LOGTAIL_ALARM_NUM`为新的总数
- 在`mMessageType`向量中添加对应的字符串
- 使用等级化接口，避免使用旧的`SendAlarm`方法

### Go新增告警类型步骤

1. **在 `pkg/selfmonitor/alarm_constants.go` 中添加告警类型**:

    ```go
    const (
        // ... 现有类型
        NewAlarmType AlarmType = "NEW_ALARM_TYPE"
    )
    ```

2. **使用告警**:

    ```go
    // 在需要发送告警的地方
    func (p *Plugin) ProcessData() error {
        if err := p.doSomething(); err != nil {
            // 使用等级化接口发送告警
            logger.Warning(ctx, selfmonitor.NewAlarmType, 
                fmt.Sprintf("处理数据失败: %v", err))
            return err
        }
        
        // 或者使用其他等级
        if p.isCriticalError() {
            logger.Critical(ctx, selfmonitor.NewAlarmType, 
                "严重错误，需要立即处理")
        }
        
        return nil
    }
    ```

**重要注意事项**:

- 告警类型字符串必须与C++中的枚举名称一致
- 使用等级化接口：`Warning`、`Error`、`Critical`
- 告警消息应该包含具体的错误信息
- 避免在循环中频繁发送相同告警

## 代码风格规范

### C++代码风格

- **命名规范**:
  - 类名使用PascalCase: `AlarmManager`, `MetricRecord`
  - 函数名使用camelCase: `SendAlarmWarning`, `GetInstance`
  - 常量使用SCREAMING_SNAKE_CASE: `METRIC_AGENT_CPU`
  - 成员变量使用m前缀: `mAlarmBufferMutex`, `mAllAlarmMap`

- **代码结构**:
  - 头文件声明与实现分离
  - 使用namespace logtail包装
  - 适当的const和constexpr使用

### Go代码风格

- **命名规范**:
  - 包名使用小写: `selfmonitor`
  - 类型名使用PascalCase: `AlarmType`, `AlarmLevel`
  - 函数名使用camelCase: `Record`, `SerializeToPb`
  - 常量使用SCREAMING_SNAKE_CASE: `AlarmLevelWarning`

- **代码结构**:
  - 适当的错误处理
  - 使用sync.Mutex保护并发访问
  - 清晰的注释和文档

## 指标使用最佳实践

### 指标创建最佳实践

1. **在合适的生命周期创建指标**:

    **C++示例**:

    ```cpp
    class Plugin {
    private:
        MetricsRecordRef mMetricsRecordRef;
        CounterPtr mProcessedCounter;
        IntGaugePtr mQueueSizeGauge;
        
    public:
        bool Init() {
            // 在Init方法中创建MetricsRecordRef和指标对象
            MetricLabelsPtr labels = std::make_shared<MetricLabels>();
            labels->emplace_back(METRIC_LABEL_KEY_PLUGIN_TYPE, "input_file");
            
            WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
                mMetricsRecordRef, std::move(labels), nullptr);
            
            // 创建指标对象
            mProcessedCounter = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_PROCESSED_TOTAL);
            mQueueSizeGauge = mMetricsRecordRef.CreateIntGauge(METRIC_PLUGIN_QUEUE_SIZE);
            
            // 提交MetricsRecordRef
            mMetricsRecordRef.Commit();
            return true;
        }
    };
    ```

    **Go示例**:

    ```go
    type Plugin struct {
        pipeline.PluginContext
        processedCounter selfmonitor.CounterMetric
        queueSizeGauge   selfmonitor.GaugeMetric
    }

    func (p *Plugin) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
        // 获取插件通用labels
        labels := pipeline.GetPluginCommonLabels(p.Config.Context, pluginMeta)
        
        // 注册MetricsRecord
        p.MetricRecord = p.Config.Context.RegisterMetricRecord(labels)
        
        // 创建并注册指标对象
        p.processedCounter = selfmonitor.NewCounterMetricAndRegister(p.MetricRecord, selfmonitor.MetricPluginProcessedTotal)
        p.queueSizeGauge = selfmonitor.NewGaugeMetricAndRegister(p.MetricRecord, selfmonitor.MetricPluginQueueSize)
    }
    ```

2. **使用安全的更新宏**:

    ```cpp
    // 使用宏确保指针非空
    void Plugin::ProcessData() {
        // 检查MetricsRecordRef状态
        if (mMetricsRecordRef.IsCommitted()) {
            ADD_COUNTER(mProcessedCounter, 1);
            SET_GAUGE(mQueueSizeGauge, currentQueueSize);
        }
    }

    // 错误示例：直接调用可能为空指针
    void Plugin::ProcessDataWrong() {
        mProcessedCounter->Add(1);  // 危险：可能为空指针
    }
    ```

3. **避免频繁创建指标对象**:

    ```cpp
    // 正确：在初始化时创建一次
    class Plugin {
        CounterPtr mCounter;  // 成员变量，只创建一次
        
    public:
        void Init() {
            mCounter = mMetricsRecordRef.CreateCounter(METRIC_NAME);
            mMetricsRecordRef.Commit();
        }
        
        void ProcessData() {
            ADD_COUNTER(mCounter, 1);  // 重复使用
        }
    };

    // 错误：每次调用都创建新指标
    void ProcessDataWrong() {
        auto counter = mMetricsRecordRef.CreateCounter(METRIC_NAME);  // 错误：频繁创建
        counter->Add(1);
    }
    ```

### 告警使用最佳实践

1. **选择合适的告警等级**:

    ```cpp
    // 正确：根据影响程度选择等级
    void ProcessData() {
        if (parseError) {
            // warning: 单点解析失败，不影响整体流程
            AlarmManager::GetInstance()->SendAlarmWarning(
                PARSE_LOG_FAIL_ALARM, 
                "单行解析失败: " + errorLine
            );
        }
        
        if (queueFull) {
            // error: 队列满，影响处理流程
            AlarmManager::GetInstance()->SendAlarmError(
                PROCESS_QUEUE_BUSY_ALARM, 
                "处理队列已满，当前大小: " + std::to_string(queueSize)
            );
        }
        
        if (configLoadFailed) {
            // error: 单个采集配置加载失败，影响一个流水线，可能导致客户资损
            AlarmManager::GetInstance()->SendAlarmCritical(
                CATEGORY_CONFIG_ALARM, 
                "采集配置加载失败: " + configError
            );
        }
    }
    ```

2. **提供有意义的告警消息**:

    ```cpp
    // 正确：包含具体错误信息和解决建议
    void HandleFileError(const std::string& filePath, int errorCode) {
        std::string message = "文件读取失败: " + filePath + 
                            ", 错误码: " + std::to_string(errorCode) +
                            ", 建议检查文件权限和路径";
        
        AlarmManager::GetInstance()->SendAlarmError(
            OPEN_LOGFILE_FAIL_ALARM, 
            message
        );
    }

    // 错误：告警消息过于简单
    void HandleFileErrorWrong(const std::string& filePath) {
        AlarmManager::GetInstance()->SendAlarmError(
            OPEN_LOGFILE_FAIL_ALARM, 
            "文件错误"  // 太简单，无法定位问题
        );
    }
    ```

3. **避免告警风暴**:

    ```cpp
    // 正确：使用限流机制避免告警风暴
    class AlarmLimiter {
    private:
        std::map<AlarmType, time_t> mLastAlarmTime;
        static const int ALARM_INTERVAL_SEC = 60;  // 60秒内不重复发送相同告警
        
    public:
        void SendAlarmWithLimit(AlarmType type, const std::string& message) {
            time_t now = time(nullptr);
            auto it = mLastAlarmTime.find(type);
            
            if (it == mLastAlarmTime.end() || 
                now - it->second > ALARM_INTERVAL_SEC) {
                
                AlarmManager::GetInstance()->SendAlarmError(type, message);
                mLastAlarmTime[type] = now;
            }
        }
    };

    // 错误：在循环中频繁发送相同告警
    void ProcessDataWrong() {
        for (auto& item : dataList) {
            if (item.hasError) {
                // 错误：可能产生大量重复告警
                AlarmManager::GetInstance()->SendAlarmError(
                    PROCESS_DATA_FAIL_ALARM, 
                    "数据处理失败"
                );
            }
        }
    }
    ```

## 检查清单

在提交涉及自监控的代码前，请确保：

1. **指标命名**:

   - [ ] 指标名称符合命名规范
   - [ ] 使用了正确的模块前缀
   - [ ] 单位使用标准格式
   - [ ] Label名称符合命名规范

2. **告警使用**:

   - [ ] 使用正确的告警等级接口
   - [ ] 告警等级与严重程度匹配
   - [ ] 避免使用已废弃的接口
   - [ ] 告警消息有意义

3. **代码实现**:

   - [ ] 指标能正常创建和更新
   - [ ] 告警能正常记录和聚合
   - [ ] 使用了安全的更新宏
   - [ ] 错误处理完善

4. **性能考虑**:

   - [ ] 避免频繁创建指标对象
   - [ ] 避免告警风暴
   - [ ] 指标更新不会影响主流程性能

5. **测试验证**:

   - [ ] 新增指标能正常上报
   - [ ] 新增告警能正常触发
   - [ ] 数据格式符合预期
   - [ ] 性能影响可接受
