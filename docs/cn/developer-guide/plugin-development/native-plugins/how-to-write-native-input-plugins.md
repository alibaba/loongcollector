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

**参考说明**：

- **Journal Server**：使用`atomic`的CAS操作（`compare_exchange_strong`）确保线程安全，最严格
- **eBPF Server**：使用`atomic_bool`进行简单检查和赋值
- **File Server**：直接启动，依赖ConfigManager的单例特性
- **Container Manager**：使用简单bool标志，因为Init通常只调用一次

### 反压处理

当下游处理队列繁忙时，输入插件应当主动进行反压控制，避免内存无限增长。

#### 核心机制：队列状态检查

所有反压策略的基础是在推送前检查队列状态：

```c++
// 检查队列是否可推送
if (ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
    // 队列可用，可以推送数据
    PushEventGroup(...);
} else {
    // 队列繁忙，根据策略处理（累积/跳过/重试）
}
```

**IsValidToPush机制**：

- 基于bounded queue的**高低水位线**（high/low watermark）
- 队列达到高水位线时返回false，降到低水位线时恢复true
- 避免频繁的状态切换

#### 三种反压策略

根据数据源的读取特性，有三种主要的反压处理策略：

**策略1：累积 + 队列检查**（小粒度读取）

**适用场景**：数据源API只能逐条读取（如systemd journal的entry-by-entry）

**为什么需要累积**：journal API每次只能读取一个entry，需要累积到一定数量才推送

```c++
// 累积数据到EventGroup
std::shared_ptr<PipelineEventGroup> accumulatedEventGroup;
int accumulatedCount = 0;
const int maxBatchSize = 1000;

while (accumulatedCount < maxBatchSize) {
    Entry entry = ReadOneEntry();  // 只能一次读一条
    AddToEventGroup(entry, accumulatedEventGroup);
    accumulatedCount++;
}

// 推送时机判断
bool timeout = (now - lastPushTime) > batchTimeout;
bool reachedMax = accumulatedCount >= maxBatchSize;

if (timeout || reachedMax) {
    // 必须推送：超时或达到上限
    PushEventGroup(...);
} else if (hasAccumulatedData && IsValidToPush(queueKey)) {
    // 队列可用且有数据，可以推送
    PushEventGroup(...);
} else {
    // 保留累积数据，下次继续
}
```

**推送触发条件**：

1. 达到批次上限（如1000条）
2. 超时触发（如1秒）
3. 无新数据但队列可用

**实际应用**：Journal Server

**策略2：时间片 + 即时推送**（chunk读取）

**适用场景**：数据源可以批量读取（如文件chunk读取）

**为什么不需要累积**：文件读取API（如`read()`）一次可以读取多KB数据（包含多行日志），无需再累积

```c++
for (auto& pipeline : pipelines) {
    auto startTime = now();
    
    // 每个pipeline分配固定时间片（如50ms）
    while (now() - startTime < 50ms) {
        // 每次读取前检查队列
        if (!IsValidToPush(queueKey)) {
            break;  // 队列繁忙，跳过
        }
        
        // 批量读取（一次读取chunk，包含多行日志）
        auto logBuffer = ReadChunk();  
        
        // 立即生成EventGroup并推送
        auto group = GenerateEventGroup(logBuffer);
        PushQueue(queueKey, std::move(group));
        
        if (noMoreData) {
            break;  // 数据读完
        }
    }
}
```

**关键特点**：

- **时间片控制**：每个pipeline限时（如50ms），保证多配置公平
- **即时推送**：读一批推一批，降低延迟
- **无内存累积**：不在应用层累积，节省内存

**实际应用**：File Server（StaticFileServer）

**策略3：重试缓存**（异步事件处理）

**适用场景**：事件处理需要异步依赖（如进程信息查询、容器信息查询）

**为什么需要重试**：eBPF事件到达时，所需的进程/容器信息可能还未准备好，需要重试

```c++
// 尝试处理并推送事件
if (!TryProcessAndPushEvent(event)) {
    // 处理失败（如依赖数据未就绪），加入重试缓存
    retryCache.AddEvent(event);
}

// 定期重试缓存中的事件
void HandleRetryCache() {
    for (auto& event : retryCache) {
        if (event->Retry()) {
            // 重试成功，移除
            retryCache.Remove(event);
        } else {
            event->DecrementRetryCount();
            if (!event->CanRetry()) {
                // 超过重试次数，丢弃并告警
                event->OnDrop();
                retryCache.Remove(event);
            }
        }
    }
}
```

**实际应用**：eBPF Server（RetryableEventCache）

#### 推送重试机制

无论使用哪种策略，推送时都应使用重试机制：

```c++
constexpr uint32_t kMaxRetries = 100;    // 最多重试100次
constexpr uint32_t kRetryInterval = 10;  // 重试间隔10ms

// ProcessorRunner内置重试逻辑
if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, inputIdx, 
                                               std::move(group), kMaxRetries)) {
    LOG_ERROR(sLogger, ("push failed after retries", "queue may be full"));
    return false;  // 最终失败，记录告警
}
```

**重试参数建议**：

- 重试次数：100次（约1秒）
- 重试间隔：10ms
- 失败处理：记录告警，考虑丢弃或持久化

#### EventGroup分组策略

合理的EventGroup大小可以平衡吞吐量和内存占用：

- **批次大小**：通常1000-5000条事件
- **超时时间**：1-3秒（保证实时性）
- **触发条件**：达到批次上限 OR 超时触发 OR 队列恢复可用

#### 策略选择指南

| 数据源特性 | 推荐策略 | 代表实现 |
|-----------|---------|---------|
| entry-by-entry读取 | 累积 + 队列检查 | Journal Server |
| chunk批量读取 | 时间片 + 即时推送 | File Server |
| 异步事件处理 | 重试缓存 | eBPF Server |

### 插件退出细节

优雅的插件退出需要处理三个核心问题：**停止信号传递**、**线程唤醒**、**资源清理**。

#### 退出流程的四个步骤

```c++
void XxxInputRunner::Stop() {
    // 1. 防止重复停止
    if (!CheckAndSetStopping()) {
        return;
    }

    // 2. 发送停止信号
    SendStopSignal();

    // 3. 等待线程退出（带超时）
    WaitForThreadExit(timeout);

    // 4. 清理资源
    CleanupResources();
}
```

#### 三种停止信号机制

**机制1：原子标志 + 自然唤醒**（适用于阻塞等待场景）

**适用场景**：线程在epoll_wait、sleep等阻塞调用上等待

**为什么选择**：epoll_wait会定期超时返回，无需额外唤醒机制

```c++
// Stop()函数
void Stop() {
    mIsRunning.store(false);  // 设置停止标志
    
    // 等待线程自然唤醒并退出
    auto status = mThreadRes.wait_for(std::chrono::seconds(10));
    if (status != std::future_status::ready) {
        LOG_WARNING(sLogger, ("thread stop timeout", "may leak resources"));
    }
}

// 工作线程
void run() {
    while (mIsRunning.load()) {
        // epoll_wait会定期超时返回（如200ms）
        int n = epoll_wait(epollFd, events, maxEvents, timeoutMs);
        // 处理事件...
        // 循环回到while检查mIsRunning
    }
}
```

**超时选择**：10秒（给足够时间让阻塞调用返回）

**实际应用**：Journal Server、eBPF Server（epoll_wait阻塞）

**机制2：条件变量 + 主动唤醒**（适用于短周期任务）

**适用场景**：线程在条件变量上等待，需要快速响应停止信号

**为什么选择**：短周期任务（如10ms轮询）需要立即唤醒，不等待自然超时

```c++
// Stop()函数
void Stop() {
    {
        lock_guard<mutex> lock(mMutex);
        mIsRunning = false;
    }
    mStopCV.notify_all();  // 主动唤醒线程
    
    // 短超时即可（线程会立即被唤醒）
    auto status = mThreadRes.wait_for(std::chrono::seconds(1));
}

// 工作线程
void run() {
    unique_lock<mutex> lock(mMutex);
    while (mIsRunning) {
        lock.unlock();
        DoWork();  // 执行任务
        lock.lock();
        
        // 在条件变量上等待，可被notify_all立即唤醒
        if (mStopCV.wait_for(lock, chrono::milliseconds(10), 
                            [this]() { return !mIsRunning; })) {
            return;  // 收到停止信号，立即退出
        }
    }
}
```

**超时选择**：1秒（条件变量会立即唤醒，无需长等待）

**实际应用**：File Server（StaticFileServer每10ms轮询）

**机制3：简单标志 + 长周期轮询**（适用于低频任务）

**适用场景**：线程执行低频任务（如3秒轮询），对退出延迟不敏感

**为什么选择**：任务本身周期长，简化实现，接受较长退出延迟

```c++
// Stop()函数
void Stop() {
    mIsRunning = false;  // 简单设置标志
    
    try {
        // 等待时间略长于轮询周期
        auto status = mThreadRes.wait_for(std::chrono::seconds(5));
        if (status != std::future_status::ready) {
            LOG_WARNING(sLogger, ("thread stop timeout", ""));
        }
    } catch (...) {
        LOG_ERROR(sLogger, ("stop thread failed", ""));
    }
}

// 工作线程
void run() {
    while (mIsRunning) {
        DoWork();  // 执行任务
        std::this_thread::sleep_for(std::chrono::seconds(3));  // 长周期sleep
        // 循环回到while检查mIsRunning
    }
}
```

**超时选择**：5秒（略长于轮询周期3秒）

**实际应用**：Container Manager（每3秒轮询容器状态）

#### 资源清理要点

```c++
void Stop() {
    // 1. 防止重复停止（使用CAS更安全）
    bool expected = true;
    if (!mIsInitialized.compare_exchange_strong(expected, false)) {
        return;
    }

    // 2. 发送停止信号并等待
    mIsRunning.store(false);
    // ... 唤醒和等待 ...

    // 3. 清理资源
    // 保存状态（如checkpoint）
    SaveCheckpoint();
    
    // 清理连接和句柄
    CloseConnections();
    
    // 释放内存
    ClearCaches();
    
    LOG_INFO(sLogger, ("runner stopped", ""));
}

// 工作线程退出前清理
void run() {
    while (mIsRunning) {
        // ...
    }
    
    // 退出前清理
    FlushBuffers();      // 刷新缓冲区
    SaveCheckpoint();    // 保存checkpoint
}
```

**资源清理建议**：

- **保存状态优先**：checkpoint、缓冲区数据要优先保存
- **异常保护**：使用try-catch保护清理代码
- **RAII优先**：优先使用智能指针、RAII类自动清理
- **清理顺序**：先停止数据输入，再保存状态，最后释放资源

#### 超时处理策略

不同场景需要不同的超时时间：

| 线程类型 | 超时时间 | 原因 |
|---------|---------|------|
| epoll等待 | 10秒 | 等待epoll_wait超时返回 |
| 短周期任务（10ms） | 1秒 | 条件变量立即唤醒 |
| 长周期任务（3s） | 5秒 | 等待sleep自然结束 |

**超时后的处理**：

```c++
if (status != std::future_status::ready) {
    // 记录告警
    LOG_ERROR(sLogger, ("thread stop timeout", "forced to stop")("timeout_sec", timeout));
    
    // 发送监控告警
    AlarmManager::GetInstance()->SendAlarm("THREAD_STOP_TIMEOUT", ...);
    
    // 注意：此时线程可能仍在运行，可能导致资源泄漏
    // 但继续等待会阻塞整个进程退出，需要权衡
}
```

#### 机制选择指南

| 线程特征 | 推荐机制 | 代表实现 |
|---------|---------|---------|
| epoll/select阻塞 | 原子标志 + 自然唤醒 | Journal Server, eBPF Server |
| 短周期任务（<100ms） | 条件变量 + 主动唤醒 | File Server |
| 长周期任务（>1s） | 简单标志 + 长周期轮询 | Container Manager |

### EventPool的使用

EventPool通过对象复用减少内存分配开销，适用于高频事件创建场景。

#### 为什么需要EventPool

**性能问题**：输入插件通常需要高频创建Event对象（如每秒数千到数万次），频繁的`new`/`delete`会导致：

1. **内存分配开销**：系统调用malloc/free的CPU开销
2. **内存碎片**：频繁分配释放导致堆碎片化
3. **锁竞争**：多线程下内存分配器的锁竞争

**性能提升**：使用EventPool可以：

- **减少80%以上的内存分配开销**（高频场景）
- **降低内存碎片**
- **提高缓存命中率**（对象复用）

**适用场景判断**：

- ✅ **高频场景**（>1000次/秒）：文件采集、metrics采集
- ✅ **批量创建**：单次创建大量Event
- ❌ **低频场景**（<100次/秒）：配置变更、容器发现
- ❌ **单次少量**：偶尔创建几个Event

#### 两种使用模式

**模式1：全局EventPool**（多线程共享）

**适用场景**：EventGroup会跨线程传递，或多个线程同时创建Event

**特点**：线程安全，但有锁开销

```c++
// 使用全局pool（默认方式）
PipelineEventGroup group(sourceBuffer);

// 第一个参数true表示使用EventPool
LogEvent* event = group.AddLogEvent(true);  
event->SetContent("key", "value");

// EventGroup析构时自动归还到pool
```

**内部机制**：

- 使用全局单例`gThreadedEventPool`
- 双缓冲设计（pool和poolBak）减少锁竞争
- 支持多线程并发获取和归还

**实际应用**：File Server、Host Monitor

**模式2：自定义EventPool**（单线程独占）

**适用场景**：EventGroup仅在创建线程内使用，不跨线程传递

**特点**：无锁，性能最优

```c++
class XxxInputRunner {
private:
    // false表示不加锁
    EventPool mEventPool{false};
    
    void processData() {
        PipelineEventGroup group(sourceBuffer);
        
        // 使用自定义pool（第二个参数）
        LogEvent* event = group.AddLogEvent(true, &mEventPool);
        event->SetContent("key", "value");
        
        // 在当前线程内使用group，不传递到其他线程
        ProcessLocally(group);
    }
};
```

**注意事项**：

- ⚠️ **严禁跨线程**：EventGroup不能传递到其他线程
- ⚠️ **生命周期**：EventPool生命周期必须长于所有使用它的EventGroup
- ✅ **性能最优**：无锁操作，适合高性能场景

**实际应用**：eBPF Server（handler线程独占）

#### 自动回收机制

EventPool的回收完全自动，开发者无需关心：

```c++
void ProcessData() {
    PipelineEventGroup group(sourceBuffer);
    
    // 1. 从pool获取
    LogEvent* e1 = group.AddLogEvent(true);
    LogEvent* e2 = group.AddLogEvent(true);
    
    // 2. 使用事件
    e1->SetContent("key1", "value1");
    e2->SetContent("key2", "value2");
    
    // 3. group析构时自动归还到pool
}
// e1, e2已经回到pool，可被复用
```

**回收时机**：

- EventGroup析构时
- 自动调用Event的Reset()清空数据
- 归还到pool等待下次获取

**GC机制**：

- EventPool定期GC（默认60秒）
- 释放过多的闲置对象（避免内存占用过大）
- 保留最小数量的对象供快速获取

#### 使用API

```c++
// 创建Event的两种方式
LogEvent* e1 = group.AddLogEvent(true);        // 从全局pool获取
LogEvent* e2 = group.AddLogEvent(true, &pool); // 从自定义pool获取
LogEvent* e3 = group.AddLogEvent(false);       // 使用new创建（不走pool）

// 支持的Event类型
LogEvent* log = group.AddLogEvent(true);
MetricEvent* metric = group.AddMetricEvent(true);
SpanEvent* span = group.AddSpanEvent(true);
RawEvent* raw = group.AddRawEvent(true);
```

#### 使用决策树

```text
是否高频创建Event（>1000次/秒）？
├─ 否 → 不使用EventPool（AddEvent(false)）
└─ 是 → EventGroup是否跨线程传递？
    ├─ 是 → 使用全局EventPool（AddEvent(true)）
    └─ 否 → 使用自定义EventPool（AddEvent(true, &customPool)）
```

#### 性能对比

| 场景 | 不使用Pool | 全局Pool | 自定义Pool |
|-----|----------|---------|-----------|
| 创建开销 | 100% (基准) | ~20% | ~10% |
| 锁开销 | malloc锁 | pool锁 | 无锁 |
| 适用场景 | 低频 | 多线程高频 | 单线程高频 |
| 代表实现 | - | File Server | eBPF Server |

#### 实际应用示例

**Journal Server**：

- **使用方式**：不使用EventPool
- **线程模型**：单线程（run()主循环）
- **能否使用自定义Pool**：理论可行（单线程），但**没有必要**
- **详细分析**：

  ```cpp
  // 累积阶段：创建 Event
  LogEvent* event = eventGroup.AddLogEvent();  // 使用 new
  
  // 推送阶段：Copy EventGroup
  PipelineEventGroup copy = eventGroup->Copy();  // ← 关键问题！
  PushQueue(queueKey, std::move(copy));
  
  // Copy() 实现
  for (auto& event : mEvents) {
      res.mEvents.emplace_back(event.Copy());  // 调用 Event::Copy()
  }
  
  // LogEvent::Copy() 实现
  return make_unique<LogEvent>(*this);  // 使用 new 创建新对象
  ```
  
  **为什么不建议使用EventPool**：
  
  1. **Copy机制抵消收益**：推送时使用`Copy()`创建新对象，仍需大量new操作
  2. **长期持有**：累积的Event保留在内存中等待推送，无法快速归还Pool复用
  3. **收益极小**：即使使用自定义Pool（无锁），也只能优化累积阶段（约5%收益）
  
  **性能对比**（假设每秒1000条日志）：
  
  | 方案 | 累积阶段 | 推送阶段 | 总分配次数 | 改进 |
  |------|---------|---------|-----------|------|
  | 当前（不用Pool） | 1000次new | 1000次new(Copy) | 2000次 | - |
  | 使用自定义Pool | ~100次new + 900次Pool | 1000次new(Copy) | ~1100次 | 45%↓ |
  | 理论最优（无Copy） | ~100次new + 900次Pool | 0次（直接move） | ~100次 | 95%↓ |
  
  **为什么Journal Server用Copy而File Server用Move**：
  
  **File Server（move推送）**：
  
  ```cpp
  auto group = GenerateEventGroup(reader, logBuffer);
  PushQueue(queueKey, inputIdx, std::move(group));  // move推送
  
  // 推送失败处理
  if (!pushSuccess) {
      LOG_ERROR(sLogger, ("failed to push", "discard data"));  // ← 直接丢弃！
  }
  // 立即更新checkpoint（无论成功失败）
  UpdateCheckpoint(configName, idx, reader->GetLastFilePos(), ...);
  ```
  
  **关键特点**：
  - 读取后文件位置（mLastFilePos）已前进
  - 推送失败直接丢弃数据
  - checkpoint记录新位置，进程重启时不会重复读取
  - **可接受运行时丢失**，依赖checkpoint在崩溃后恢复
  - **数据源持久化**：文件在磁盘，重启后仍可访问
  
  **Journal Server（Copy推送）**：
  
  ```cpp
  // 读取数据，cursor前进
  while (...) {
      journalReader->Next();  // cursor前进
      CreateLogEventFromJournal(entry, *eventGroup);  // 累积到内存
  }
  
  // Copy推送，保留原始数据
  PipelineEventGroup copy = eventGroup->Copy();
  if (!PushQueue(queueKey, 0, std::move(copy))) {
      // 推送失败，保留 eventGroup！
      pushFailed = true;
      return false;  // accumulatedEventGroup仍存在
  }
  // 推送成功才清理
  ClearAccumulatedData(accumulatedEventGroup, ...);
  ```
  
  **关键特点**：
  - cursor已前进，但数据保留在内存（accumulatedEventGroup）
  - 推送失败时保留数据，下次循环不读新数据，直接推送内存中的数据
  - **运行时零丢失**：数据在内存中重试，直到成功推送
  - **数据源易失**：systemd journal可能滚动删除，运行时丢失就永久丢失
  
  **设计合理性**：✅ **两种策略都合理**，取决于数据源特性
  
  | 特性 | File Server | Journal Server |
  |------|------------|---------------|
  | 数据源 | 磁盘文件（持久化） | systemd journal（可能滚动删除） |
  | 推送方式 | move（零拷贝） | Copy（保留原始数据） |
  | 失败处理 | 丢弃，依赖checkpoint恢复 | 内存重试，零丢失 |
  | checkpoint | 立即更新（不管成功失败） | 推送成功后才前进 |
  | 适用场景 | 可容忍崩溃时少量丢失 | 要求运行时零丢失 |
  
  **结论**：
  - Journal Server的Copy语义是**有意设计**，为了运行时零丢失
  - 这导致EventPool收益极小（仅累积阶段，约45%），**不值得引入复杂度**
  
  **优化方向：如果改成"推送失败就丢弃"模式，EventPool能否有效？**
  
  ✅ **可以！收益从45%提升到95%**
  
  **改造方案**：
  
  ```cpp
  // 当前：Copy模式（运行时零丢失）
  while (...) {
      LogEvent* event = eventGroup.AddLogEvent();  // ← 不用Pool
      CreateLogEventFromJournal(entry, event);
  }
  PipelineEventGroup copy = eventGroup->Copy();  // ← 1000次new
  if (!PushQueue(..., std::move(copy))) {
      return false;  // 保留eventGroup，下次重试
  }
  ClearAccumulatedData(...);
  
  // 优化：Move模式（接受运行时丢失）
  while (...) {
      LogEvent* event = eventGroup.AddLogEvent(true);  // ← 使用Pool！
      CreateLogEventFromJournal(entry, event);
  }
  if (!PushQueue(..., std::move(eventGroup))) {  // ← 直接move，不Copy
      LOG_ERROR(sLogger, ("discard data"));  // 丢弃，不重试
      return false;
  }
  // 无需清理，eventGroup已被move
  ```
  
  **改造效果对比**：
  
  | 模式 | 累积阶段 | 推送阶段 | 总开销 | EventPool收益 |
  |------|---------|---------|--------|-------------|
  | 当前Copy模式 | 1000次new | 1000次new(Copy) | 2000次 | ❌ 45%（仅累积） |
  | Move+Pool模式 | ~100次new + 900次Pool | 0次（move） | ~100次 | ✅ 95%（累积+推送） |
  
  **代价与权衡**：
  
  | 方面 | Copy模式（当前） | Move+Pool模式（优化） |
  |------|----------------|---------------------|
  | **可靠性** | ✅ 运行时零丢失（内存重试） | ❌ 推送失败即丢失 |
  | **性能** | ❌ EventPool收益45% | ✅ EventPool收益95% |
  | **复杂度** | 需要累积缓冲区 | 逻辑简化，类似File Server |
  | **数据风险** | journal滚动删除风险 | journal滚动删除风险 + 推送失败丢失 |
  
  **选择建议**：
  - **保持Copy模式**：如果业务要求运行时零丢失（例如审计日志、计费日志）
  - **改用Move+Pool模式**：如果可容忍偶尔丢失，且追求极致性能（例如调试日志、监控指标）
  
  **关键洞察**：
  - EventPool是否有效，**取决于是否需要Copy语义**
  - Copy语义（重试保证）与EventPool收益是**架构层面的冲突**
  - 优化应从**业务需求**（可靠性要求）出发，而非单纯技术优化

**File Server**：

- **使用方式**：全局EventPool
- **原因**：EventGroup在多个线程间传递
- **代码**：`group.AddLogEvent(true)` → 显式使用EventPool
- **性能提升**：日志采集场景显著减少内存分配开销

**eBPF Server**：

- **使用方式**：自定义EventPool（不加锁）
- **原因**：handler线程独立处理，不跨线程
- **代码**：`group.AddLogEvent(true, &mEventPool)` → 使用自定义Pool
- **性能提升**：内核事件处理显著提升

**Host Monitor**：

- **使用方式**：全局EventPool
- **原因**：高频metrics创建
- **代码**：`group.AddMetricEvent(true)` → 显式使用EventPool

**Container Manager**：

- **使用方式**：不使用EventPool
- **原因**：低频事件（每秒几次），性能不是瓶颈
- **说明**：不是所有场景都需要EventPool，低频场景优化收益小

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
  - **初始化**：使用atomic CAS确保线程安全的单次初始化
  - **反压处理**：数据累积 + IsValidToPush检查 + 超时/批次触发
  - **退出处理**：10秒超时等待，带告警机制
  - **EventPool**：未使用（推送时使用`Copy()`创建新对象，EventPool收益极小，详见EventPool章节分析）
  - 使用epoll实现事件驱动的数据采集，支持多配置实例共享线程

### Host Monitor（主机监控指标采集）

- **插件实现**：`core/plugin/input/InputHostMonitor.cpp`
- **管理类实现**：`core/host_monitor/HostMonitorInputRunner.cpp`
- **特点**：
  - **初始化**：atomic标志 + 线程池启动
  - **反压处理**：采集前不检查队列（metrics优先级高），推送失败直接丢弃
  - **退出处理**：3秒超时 + detached线程异步停止
  - **EventPool**：使用全局pool创建MetricEvent
  - 定时器调度，支持不同采集间隔，适合周期性指标采集

### eBPF Server（内核数据采集）

- **管理类实现**：`core/ebpf/EBPFServer.cpp`
- **特点**：
  - **初始化**：使用atomic_bool但只做简单检查（非CAS）+ epoll初始化 + 双线程启动
  - **反压处理**：事件缓存 + 重试队列
  - **退出处理**：10秒超时 + 告警
  - **EventPool**：自定义不加锁EventPool（单线程使用）
  - 多线程架构（poller + handler），适合高性能内核事件采集

### File Server（文件采集）

- **管理类实现**：`core/file_server/StaticFileServer.cpp`
- **插件实现**：`core/plugin/input/InputFile.cpp`
- **特点**：
  - **初始化**：加载checkpoint + 启动inotify + 启动polling双重监控
  - **反压处理**：时间片控制（50ms/pipeline）+ 每次读取前IsValidToPush检查
  - **退出处理**：condition_variable唤醒 + 1秒超时
  - **EventPool**：使用全局pool创建LogEvent
  - 适合日志文件采集，支持checkpoint断点续传和文件轮转

### Container Manager（容器发现与管理）

- **管理类实现**：`core/container_manager/ContainerManager.cpp`
- **插件实现**：`core/plugin/input/InputContainerStdio.cpp`
- **特点**：
  - **初始化**：简单bool标志 + 轮询线程启动
  - **反压处理**：不直接推送数据，通过diff通知各配置更新状态
  - **退出处理**：5秒超时 + try-catch异常保护
  - **EventPool**：不直接使用（由依赖它的input插件使用）
  - 适合容器环境，支持Docker/CRI多运行时和K8s过滤

**选择参考**：

- **事件驱动 + 反压**：参考 Journal Server
- **定时采集**：参考 Host Monitor
- **高性能场景**：参考 eBPF Server
- **文件处理 + checkpoint**：参考 File Server
- **外部状态监控**：参考 Container Manager
