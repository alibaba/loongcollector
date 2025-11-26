# 如何开发原生Input插件

## 工作模式

同一输入类型的所有插件实例共享同一个线程来获取数据，插件实例只负责保存插件配置。

## 接口定义

```c++
class Input : public Plugin {
public:
    // 初始化插件，入参为插件参数
    virtual bool Init(const Json::Value& config) = 0;
    // 负责向管理类注册配置
    virtual bool Start() = 0;
    // 负责向管理类注销配置
    virtual bool Stop(bool isPipelineRemoving) = 0;
};
```

## 开发步骤

1. 在plugin/input目录下新建一个Inputxxx.h和Inputxxx.cpp文件，用于派生Input接口生成具体的插件类；

2. 在Inputxxx.h文件中定义新的输入插件类Inputxxx，满足以下规范：

   a. 所有的可配置参数的权限为public，其余参数的权限均为private。

3. 在Inputxxx.cpp文件中实现`Init`函数，即根据入参初始化插件，针对非法参数，根据非法程度和影响决定是跳过该参数、使用默认值或直接拒绝加载插件。

4. 在根目录下新增一个目录，用于创建当前输入插件的管理类及其他辅助类，该管理类需要继承InputRunner接口：

```c++
class InputRunner {
public:
    // 调用点：由插件的Start函数调用
    // 作用：初始化管理类，并至少启动一个线程用于采集数据
    // 注意：该函数必须是可重入的，因此需要在函数开头判断是否已经启动线程，如果是则直接退出
    virtual void Init() = 0;
    // 调用点：进程退出时，或配置热加载结束后无注册插件时由框架调用
    // 作用：停止管理类，并进行扫尾工作，如资源回收、checkpoint记录等
    virtual void Stop() = 0;
    // 调用点：每次配置热加载结束后由框架调用
    // 作用：判断是否有插件注册，若无，则框架将调用Stop函数对线程资源进行回收
    virtual bool HasRegisteredPlugin() const = 0;
}
```

管理类是输入插件线程资源的实际拥有者，其最基本的运行流程如下：

- 依次访问每个注册的配置，根据配置情况抓取数据；

- 根据数据类型将源数据转换为PipelineEvent子类中的一种，并将一批数据组装成PipelineEventGroup；

- 将PipelineEventGroup发送到相应配置的处理队列中：

```c++
ProcessorRunner::GetInstance()->PushQueue(queueKey, inputIdx, std::move(group));
```

其中，

- queueKey是队列的key，可以从相应流水线的PipelineContext类的`GetProcessQueueKey()`方法来获取。

- inputIdx是当前数据所属输入插件在该流水线所有输入插件的位置（即配置中第几个，从0开始计数）

- group是待发送的数据包

最后，为了支持插件向管理类注册，管理类还需要提供注册和注销函数供插件使用，从性能的角度考虑，**该注册和注销过程应当是独立的，即某个插件的注册和注销不应当影响整个线程的运转**。

5. 在Inputxxx.cpp文件中实现其余接口函数：

   ```c++
   bool Inputxxx::Start() {
       // 1. 调用管理类的Start函数
       // 2. 将当前插件注册到管理类中
   }

   bool Inputxxx::Stop(bool isPipelineRemoving) {
       // 将当前插件从管理类中注销
   }
   ```

6. 在`PluginRegistry`类中注册该插件：

   a. 在pipeline/plugin/PluginRegistry.cpp文件的头文件包含区新增如下行：

   ```c++
   #include "plugin/input/Inputxxx.h"
   ```

   b. 在`PluginRegistry`类的`LoadStaticPlugins()`函数中新增如下行：

   ```c++
   RegisterInputCreator(new StaticInputCreator<Inputxxx>(), false);
   ```

   c. 在`PipelineManager`类的构造函数中注册该插件的管理类

## 开发最佳实践

### 初始化细节

管理类的`Init()`函数应当保证**可重入性**，即多次调用不会导致重复初始化。推荐使用原子操作来实现：

```c++
void XxxInputRunner::Init() {
    bool expected = false;
    if (!mIsInitialized.compare_exchange_strong(expected, true)) {
        LOG_INFO(sLogger, ("xxx runner already initialized", "skipping duplicate Init() call"));
        return;
    }

    // 1. 初始化各种资源（连接、监控器等）
    // 例如：XxxConnection::GetInstance().Initialize();
    
    // 2. 启动工作线程
    // 使用std::async启动异步任务，保存future用于后续等待
    mThreadRes = std::async(std::launch::async, &XxxInputRunner::run, this);

    LOG_INFO(sLogger, ("xxx runner initialized", ""));
}
```

**关键要点**：

- 使用`std::atomic<bool>`和`compare_exchange_strong`避免重复初始化
- 初始化资源要考虑异常处理，初始化失败时应重置标志位
- 使用`std::async`或线程池启动工作线程
- 记录日志便于调试

### 反压处理

当下游处理队列繁忙时，输入插件应当主动进行反压控制，避免内存无限增长。反压处理包括以下几个方面：

#### 1. 队列状态检查

在推送数据前，应先检查队列是否可推送：

```c++
if (ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
    // 队列可用，可以推送数据
    PushEventGroupToQueue(queueKey, eventGroup, ...);
} else {
    // 队列繁忙，暂缓推送，将数据暂存在内存中
    // 注意：需要控制内存累积上限，避免OOM
    if (accumulatedEntryCount != nullptr) {
        *accumulatedEntryCount = totalEntryCount;
    }
    return false;
}
```

**关键要点**：

- `IsValidToPush()`方法基于bounded queue的高低水位线机制
- 当队列达到高水位线（high watermark）时，`IsValidToPush()`返回false
- 当队列降到低水位线（low watermark）时，恢复为true
- 这种机制避免了频繁的状态切换

#### 2. EventGroup分组策略

合理控制每个EventGroup的大小，既要保证吞吐量，又要避免单个batch过大：

```c++
// 在采集循环中控制批次大小
const int maxEntriesPerBatch = config.mMaxEntriesPerBatch; // 例如 1000
int totalEntryCount = 0;

while (totalEntryCount < maxEntriesPerBatch) {
    // 读取数据并添加到eventGroup
    CreateLogEventFromSource(source, *eventGroup);
    totalEntryCount++;
    
    // 检查是否达到批次上限
    if (totalEntryCount >= maxEntriesPerBatch) {
        break;
    }
}
```

**分组触发条件**：

1. **达到批次上限**：累积的事件数达到`maxEntriesPerBatch`，立即推送
2. **超时触发**：距离上次推送超过一定时间（如1秒），即使未满也推送
3. **队列恢复可用**：队列从繁忙恢复，且有累积数据时推送

#### 3. 数据累积与推送

当队列繁忙时，需要在内存中累积数据，待队列恢复时推送：

```c++
// 使用shared_ptr管理累积的EventGroup
std::shared_ptr<PipelineEventGroup> accumulatedEventGroup;
int accumulatedEntryCount = 0;

// 累积逻辑
if (!accumulatedEventGroup) {
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    accumulatedEventGroup = std::make_shared<PipelineEventGroup>(sourceBuffer);
}

// 推送时机判断
bool timeoutTrigger = (now - lastBatchTime) > batchTimeout;
bool reachedMaxBatch = totalEntryCount >= maxEntriesPerBatch;

if ((timeoutTrigger && totalEntryCount > 0) || reachedMaxBatch) {
    // 必须推送：超时或达到上限
    PushEventGroupToQueue(...);
} else if (noNewData && totalEntryCount > 0) {
    // 无新数据但有累积数据：仅当队列可用时推送
    if (ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
        PushEventGroupToQueue(...);
    } else {
        // 保留累积数据，下次再试
        return false;
    }
}
```

#### 4. 推送重试机制

推送失败时应使用指数退避或固定间隔重试：

```c++
bool PushEventGroupToQueue(QueueKey queueKey, PipelineEventGroup& eventGroup, ...) {
    constexpr uint32_t kMaxPushRetries = 100; // 最多重试100次
    constexpr uint32_t kRetryIntervalMs = 10; // 重试间隔10ms
    
    // 使用ProcessorRunner推送，内置重试逻辑
    if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, 0, std::move(eventGroup), kMaxPushRetries)) {
        LOG_ERROR(sLogger, ("failed to push to queue", "queue may be full")("entry_count", totalEntryCount));
        return false;
    }
    return true;
}
```

**关键要点**：

- 重试次数不宜过多，避免阻塞采集线程
- 每次重试间隔10ms，100次重试约1秒
- 如果重试失败，应记录告警，考虑丢弃数据或持久化到本地

### 插件退出细节

管理类的`Stop()`函数负责优雅停止线程和清理资源：

```c++
void XxxInputRunner::Stop() {
    // 1. 使用atomic操作避免重复停止
    bool expected = true;
    if (!mIsInitialized.compare_exchange_strong(expected, false)) {
        return; // 已经停止或未初始化
    }

    if (!mThreadRes.valid()) {
        return;
    }

    // 2. 设置停止标志，通知工作线程退出
    mIsThreadRunning.store(false);

    // 3. 等待线程退出（带超时）
    if (mThreadRes.valid()) {
        auto status = mThreadRes.wait_for(std::chrono::seconds(10));
        if (status == std::future_status::ready) {
            mThreadRes.get(); // 获取结果，释放资源
            LOG_INFO(sLogger, ("xxx runner thread stopped", "successfully"));
        } else {
            LOG_ERROR(sLogger, ("xxx runner thread stop timeout", "forced to stop"));
            // 注意：强制停止可能导致资源泄漏
        }
    }

    // 4. 清理其他资源
    // 例如：XxxConnection::GetInstance().Cleanup();
    // 例如：关闭文件描述符、释放内存等

    LOG_INFO(sLogger, ("xxx runner stopped", ""));
}
```

**工作线程的退出循环**：

```c++
void XxxInputRunner::run() {
    while (mIsThreadRunning.load()) {
        try {
            // 执行数据采集任务
            // ...
            
            // 定期检查停止标志
            if (!mIsThreadRunning.load()) {
                break;
            }
        } catch (const std::exception& e) {
            LOG_ERROR(sLogger, ("xxx runner exception", e.what()));
        }
    }
    
    // 退出前的清理工作
    // 例如：保存checkpoint、清空缓冲区等
}
```

**关键要点**：

- 使用`atomic_bool`实现线程间的停止信号传递
- 使用`future.wait_for()`带超时等待，避免永久阻塞
- 考虑在超时后发送告警，记录异常
- 清理资源要考虑异常安全性，使用RAII或try-catch
- 插件的`Stop()`函数应及时从管理类注销，避免管理类访问已销毁的对象

### EventPool的使用

EventPool用于复用Event对象，减少频繁的内存分配和释放开销。

#### 1. 创建使用EventPool的事件

推荐使用`AddXxxEvent(true)`方法直接添加事件到EventGroup：

```c++
// 创建PipelineEventGroup
PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());

// 从EventPool获取并添加LogEvent
LogEvent* logEvent = eventGroup.AddLogEvent(true); // true表示从pool获取
logEvent->SetContent("key", "value");
logEvent->SetTimestamp(timestamp);

// 从EventPool获取并添加MetricEvent
MetricEvent* metricEvent = eventGroup.AddMetricEvent(true);
metricEvent->SetName("cpu_usage");
metricEvent->SetValue(75.5);
```

#### 2. 自动回收机制

EventPool的回收是自动的，无需手动调用：

```c++
{
    PipelineEventGroup eventGroup(sourceBuffer);
    LogEvent* event = eventGroup.AddLogEvent(true); // 从pool获取
    // ... 使用event ...
    
    // eventGroup析构时，会自动将event归还到pool
}
// 此时event已经回收，可被其他EventGroup复用
```

**关键要点**：

- `AddXxxEvent(true)`的第一个参数`fromPool=true`表示从pool分配
- `AddXxxEvent(false)`表示使用new分配，析构时会delete
- 默认使用全局的`gThreadedEventPool`，支持多线程安全
- EventPool使用双缓冲机制（pool和poolBak）减少锁竞争
- 仅在高性能场景下使用EventPool，普通场景差异不大

#### 3. 自定义EventPool

对于单线程场景，可以使用不加锁的EventPool提升性能：

```c++
class XxxInputRunner {
private:
    EventPool mEventPool{false}; // false表示不加锁
    
public:
    void collectData() {
        PipelineEventGroup eventGroup(sourceBuffer);
        
        // 使用自定义EventPool
        LogEvent* event = eventGroup.AddLogEvent(true, &mEventPool);
        // ... 填充数据 ...
    }
};
```

**注意事项**：

- 不加锁的EventPool仅能在单线程中使用
- 如果EventGroup可能跨线程传递，必须使用加锁的EventPool
- EventPool会定期GC（默认60秒），释放过多的闲置对象

### 性能优化建议

1. **批量处理**：尽量批量读取和推送数据，减少系统调用和锁开销
2. **避免拷贝**：使用`std::move`转移所有权，避免不必要的拷贝
3. **内存复用**：使用EventPool复用Event对象，使用SourceBuffer复用字符串
4. **日志级别**：在高频路径上使用`LOG_DEBUG`，避免`LOG_INFO`影响性能
5. **Metrics上报**：适度上报metrics，过多的metrics会影响性能
6. **异常处理**：在采集循环中捕获异常，避免单个错误导致整个线程退出

## 参考示例

以下是一些实际的输入插件实现，可作为开发参考：

### Journal Server（systemd日志采集）

- **插件实现**：`core/plugin/input/InputJournal.cpp`
- **管理类实现**：`core/journal_server/JournalServer.cpp`
- **特点**：
  - 使用epoll实现事件驱动的数据采集
  - 完善的反压处理和数据累积机制
  - 支持多配置实例共享同一线程

### Host Monitor（主机监控指标采集）

- **插件实现**：`core/plugin/input/InputHostMonitor.cpp`
- **管理类实现**：`core/host_monitor/HostMonitorInputRunner.cpp`
- **特点**：
  - 使用线程池并发采集多种监控指标
  - 定时器调度，支持不同采集间隔
  - 使用EventPool优化MetricEvent分配

### eBPF Server（内核数据采集）

- **管理类实现**：`core/ebpf/EBPFServer.cpp`
- **特点**：
  - 多线程架构：poller线程 + handler线程
  - 使用epoll监控perf buffer
  - 事件缓存和重试机制
  - 自定义EventPool避免锁竞争

这些实现展示了不同场景下的最佳实践，建议在开发新插件时参考相似的场景。
