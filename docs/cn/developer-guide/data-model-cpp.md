# 数据模型（C++）

LoongCollector C++ 部分支持 `Pipeline Event` 数据模型，这是一个可扩展的可观测性数据模型，支持 Metrics、Trace、Logging、Raw 等多种数据类型。

## PipelineEvent

`PipelineEvent` 是数据处理管道中的抽象基类，所有事件类型都继承自该基类。

```cpp
class PipelineEvent {
public:
    enum class Type { NONE, LOG, METRIC, SPAN, RAW };
    
    virtual ~PipelineEvent() = default;
    virtual std::unique_ptr<PipelineEvent> Copy() const = 0;
    virtual void Reset();
    
    Type GetType() const;
    time_t GetTimestamp() const;
    std::optional<uint32_t> GetTimestampNanosecond() const;
    void SetTimestamp(time_t t);
    void SetTimestamp(time_t t, uint32_t ns);
    std::shared_ptr<SourceBuffer>& GetSourceBuffer();
    
    virtual size_t DataSize() const;
};
```

### 事件类型

| 类型 | 说明 | 用途 |
|------|------|------|
| `LOG` | 日志事件 | 存储结构化或非结构化的日志数据 |
| `METRIC` | 指标事件 | 存储时序指标数据 |
| `SPAN` | 链路事件 | 存储分布式追踪数据 |
| `RAW` | 原始事件 | 存储未经解析的原始数据 |

## PipelineEventGroup

`PipelineEventGroup` 是事件的聚合容器，包含一组事件及其共享的元数据和标签。

```cpp
class PipelineEventGroup {
public:
    // 创建不同类型的事件
    LogEvent* AddLogEvent(bool fromPool = false, EventPool* pool = nullptr);
    MetricEvent* AddMetricEvent(bool fromPool = false, EventPool* pool = nullptr);
    SpanEvent* AddSpanEvent(bool fromPool = false, EventPool* pool = nullptr);
    RawEvent* AddRawEvent(bool fromPool = false, EventPool* pool = nullptr);
    
    // 事件管理
    const EventsContainer& GetEvents() const;
    EventsContainer& MutableEvents();
    void SwapEvents(EventsContainer& other);
    void ReserveEvents(size_t size);
    
    // 元数据管理
    void SetMetadata(EventGroupMetaKey key, StringView val);
    StringView GetMetadata(EventGroupMetaKey key) const;
    bool HasMetadata(EventGroupMetaKey key) const;
    void DelMetadata(EventGroupMetaKey key);
    
    // 标签管理
    void SetTag(StringView key, StringView val);
    StringView GetTag(StringView key) const;
    bool HasTag(StringView key) const;
    void DelTag(StringView key);
    
    // 内存管理
    std::shared_ptr<SourceBuffer>& GetSourceBuffer();
    void AddSourceBuffer(const std::shared_ptr<SourceBuffer>& sourceBuffer);
};
```

## No Copy 模式

LoongCollector C++ 数据模型的核心优化是**No Copy模式**，通过统一的内存管理机制避免频繁的字符串拷贝，显著提升性能。

### 核心组件

#### 1. StringView

`StringView` 是一个轻量级的字符串视图，不拥有数据所有权，仅保存指针和长度。

```cpp
// StringView 是 boost::string_view 的别名
using StringView = boost::string_view;
```

**特点**：

- 不分配内存，仅持有数据引用
- 用于只读访问
- 传递开销极小（16字节：指针 + 长度）

##### 可变性

`StringView` 提供的是**只读访问**，不能修改指向的数据：

```cpp
const char* str = "hello";
StringView view(str, 5);

// ✅ 可以读取
std::cout << view;           // 输出 "hello"
char first = view[0];        // 'h'

// ❌ 不能修改（编译错误）
// view[0] = 'H';            // 错误：const char* 不可修改
// view.data()[0] = 'H';     // 错误：返回 const char*
```

##### 内存分配

`StringView` **不分配任何内存**，创建和拷贝的开销极小：

```cpp
const char* original = "example data";
StringView view1(original, 12);        // 零内存分配
StringView view2 = view1;              // 仅拷贝指针和长度（16字节）
StringView view3 = view1.substr(0, 7); // 子串操作无内存分配
```

##### 生命周期管理

**关键风险**：`StringView` 不管理数据生命周期，必须确保指向的数据在使用期间有效。

```cpp
// ❌ 错误示例：悬空指针
StringView dangling;
{
    std::string temp = "temporary";
    dangling = StringView(temp);  // ⚠️ 危险！
} // temp 被销毁，dangling 变成悬空指针

std::cout << dangling;  // ❌ 未定义行为：崩溃或乱码

// ✅ 正确示例1：引用静态数据
const char* staticStr = "static string";
StringView safe1(staticStr);  // ✅ 安全：静态字符串永久有效

// ✅ 正确示例2：引用 SourceBuffer 中的数据
auto sourceBuffer = std::make_shared<SourceBuffer>();
StringBuffer buf = sourceBuffer->CopyString("data");
StringView safe2(buf.data, buf.size);  // ✅ 安全：只要 sourceBuffer 存在

// ✅ 正确示例3：在作用域内使用
const char* data = GetSomeData();
StringView view(data);
ProcessImmediately(view);  // ✅ 安全：立即使用，数据未离开作用域
```

##### 典型使用场景

**场景 1**：引用 SourceBuffer 中的数据（零拷贝）

```cpp
auto sourceBuffer = std::make_shared<SourceBuffer>();
PipelineEventGroup group(sourceBuffer);
LogEvent* log = group.AddLogEvent();

// 数据已在 SourceBuffer 中
StringBuffer keyBuf = sourceBuffer->CopyString("timestamp");
StringBuffer valBuf = sourceBuffer->CopyString("1234567890");

// 使用 StringView 引用，避免再次拷贝
StringView key(keyBuf.data, keyBuf.size);
StringView val(valBuf.data, valBuf.size);
log->SetContentNoCopy(key, val);  // 零拷贝
```

**场景 2**：字符串切片操作（无内存分配）

```cpp
// 解析日志行 "timestamp=123456 level=INFO"
StringView fullLine(rawData, lineLen);
StringView timestamp = fullLine.substr(10, 6);   // "123456"，无拷贝
StringView level = fullLine.substr(23, 4);        // "INFO"，无拷贝
```

**场景 3**：引用常量字符串

```cpp
// 设置固定的标签值
const char* region = "us-west";
metricEvent->SetTagNoCopy("region", StringView(region));
```

##### 类型转换

**从其他类型转换到 StringView**：

```cpp
// 从 StringBuffer 转换（安全）
StringBuffer buf = sourceBuffer->CopyString("hello");
StringView view1(buf.data, buf.size);

// 从 const char* 转换
const char* cstr = "world";
StringView view2(cstr);  // 自动计算长度
StringView view3(cstr, 5);  // 指定长度

// 从 std::string 转换（注意生命周期）
std::string str = "example";
StringView view4(str);  // ⚠️ 必须确保 str 不被销毁
```

**从 StringView 转换到其他类型**：

```cpp
StringView view("hello", 5);

// 转换为 std::string（拷贝）
std::string str = view.to_string();

// 转换为 StringBuffer（通过 SourceBuffer 拷贝）
StringBuffer buf = sourceBuffer->CopyString(view);
```

#### 2. StringBuffer

`StringBuffer` 是由 `SourceBuffer` 分配的可写字符串缓冲区。

```cpp
class StringBuffer {
public:
    char* const data;        // 数据指针（可写）
    size_t size;             // 实际数据长度
    const size_t capacity;   // 容量（不含 '\0'）
};
```

**特点**：

- 由 `SourceBuffer` 统一管理生命周期
- 数据指针不变，可安全转换为 `StringView`
- 自动确保末尾 `'\0'`

##### 可变性

`StringBuffer` 提供**可写访问**，可以修改缓冲区内的数据：

```cpp
StringBuffer buf = sourceBuffer->AllocateStringBuffer(100);

// ✅ 可以写入数据
strcpy(buf.data, "hello");
buf.size = 5;

// ✅ 可以修改内容
buf.data[0] = 'H';  // "Hello"

// ✅ 可以使用标准库函数
snprintf(buf.data, buf.capacity, "%s_%d", "prefix", 123);
buf.size = strlen(buf.data);

// ⚠️ 注意：不能超过 capacity
// strcpy(buf.data, very_long_string);  // 可能溢出
```

##### 内存分配

`StringBuffer` 的内存由 `SourceBuffer` 统一分配和管理：

```cpp
auto sourceBuffer = std::make_shared<SourceBuffer>();

// 方式1：分配空缓冲区，稍后写入
StringBuffer buf1 = sourceBuffer->AllocateStringBuffer(100);
// 分配 101 字节（100 + 1 for '\0'）
// buf1.data[100] = '\0' 已自动设置

// 方式2：拷贝现有数据
StringBuffer buf2 = sourceBuffer->CopyString("hello");
// 自动分配 6 字节（5 + 1 for '\0'）
// 数据已拷贝到 SourceBuffer 管理的内存中
```

**内存分配特点**：

```cpp
// Chunk 内连续分配（高效）
StringBuffer s1 = sourceBuffer->CopyString("a");      // Chunk 内分配
StringBuffer s2 = sourceBuffer->CopyString("b");      // 紧接着分配
StringBuffer s3 = sourceBuffer->CopyString("c");      // 继续分配
// s1, s2, s3 的数据在内存中相邻，CPU 缓存友好

// 大对象独立分配
StringBuffer large = sourceBuffer->AllocateStringBuffer(10000);
// 超过 Chunk 一半，独立分配，避免浪费
```

##### 生命周期管理

`StringBuffer` 的生命周期由 `SourceBuffer` 管理，随 `PipelineEventGroup` 一起释放：

```cpp
// ✅ 正确示例：标准使用模式
auto sourceBuffer = std::make_shared<SourceBuffer>();
PipelineEventGroup group(sourceBuffer);

StringBuffer buf = sourceBuffer->CopyString("data");
LogEvent* log = group.AddLogEvent();
log->SetContentNoCopy("key", StringView(buf.data, buf.size));

// group 存在期间，buf 中的数据始终有效
ProcessGroup(group);  // ✅ 安全
```

```cpp
// ⚠️ 注意：StringBuffer 离开 SourceBuffer 的生命周期
StringBuffer orphan;
{
    auto tempBuffer = std::make_shared<SourceBuffer>();
    orphan = tempBuffer->CopyString("data");
} // tempBuffer 销毁，orphan.data 变为悬空指针

// orphan.data 现在无效
```

**最佳实践**：

```cpp
// ✅ 推荐：StringBuffer 和 SourceBuffer 一起管理
class MyPlugin {
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    std::vector<StringBuffer> mCachedStrings;
    
public:
    void Process() {
        mSourceBuffer = std::make_shared<SourceBuffer>();
        mCachedStrings.push_back(mSourceBuffer->CopyString("cached"));
        // mSourceBuffer 存在，所有 StringBuffer 都有效
    }
};
```

##### 典型使用场景

**场景 1**：拷贝外部数据到 SourceBuffer

```cpp
// 数据来自临时变量或外部系统
std::string external = ReadFromNetwork();

// 拷贝到 SourceBuffer，之后 external 可以安全销毁
StringBuffer buf = sourceBuffer->CopyString(external);
logEvent->SetContentNoCopy("message", StringView(buf.data, buf.size));
```

**场景 2**：构造新字符串

```cpp
// 需要拼接或格式化字符串
StringBuffer buf = sourceBuffer->AllocateStringBuffer(256);

// 方式1：使用 snprintf
int len = snprintf(buf.data, buf.capacity, "%s_%s_%d", 
                   prefix, suffix, timestamp);
buf.size = len;

// 方式2：手动拼接
strcpy(buf.data, "prefix_");
strcat(buf.data, suffix);
buf.size = strlen(buf.data);

// 设置到事件中
metricEvent->SetNameNoCopy(StringView(buf.data, buf.size));
```

**场景 3**：修改字符串内容

```cpp
StringBuffer buf = sourceBuffer->CopyString("hello world");

// 转换为大写
for (size_t i = 0; i < buf.size; ++i) {
    buf.data[i] = toupper(buf.data[i]);
}
// 现在是 "HELLO WORLD"

// 替换字符
for (size_t i = 0; i < buf.size; ++i) {
    if (buf.data[i] == ' ') {
        buf.data[i] = '_';
    }
}
// 现在是 "HELLO_WORLD"
```

**场景 4**：从文件读取数据

```cpp
// 直接读入 SourceBuffer
size_t fileSize = GetFileSize(fd);
StringBuffer buf = sourceBuffer->AllocateStringBuffer(fileSize);

ssize_t bytesRead = read(fd, buf.data, buf.capacity);
buf.size = bytesRead;

// 数据已在 SourceBuffer 中，可直接使用
rawEvent->SetContentNoCopy(StringView(buf.data, buf.size));
```

##### 类型转换

**从其他类型转换到 StringBuffer**：

```cpp
// 从 std::string（拷贝）
std::string str = "hello";
StringBuffer buf1 = sourceBuffer->CopyString(str);

// 从 const char*（拷贝）
const char* cstr = "world";
StringBuffer buf2 = sourceBuffer->CopyString(cstr, strlen(cstr));

// 从 StringView（拷贝）
StringView view("example", 7);
StringBuffer buf3 = sourceBuffer->CopyString(view);
```

**从 StringBuffer 转换到其他类型**：

```cpp
StringBuffer buf = sourceBuffer->CopyString("hello");

// 转换为 StringView（零拷贝）
StringView view(buf.data, buf.size);

// 转换为 std::string（拷贝）
std::string str(buf.data, buf.size);

// 转换为 const char*（可能不是 null-terminated，需注意）
const char* cstr = buf.data;  // ⚠️ 虽然末尾有 '\0'，但 API 使用时建议明确传递 size
```

##### StringView vs StringBuffer 选择指南

| 场景 | 使用 StringView | 使用 StringBuffer |
|------|----------------|------------------|
| 数据来源 | 已在 SourceBuffer 或静态存储 | 外部临时数据 |
| 是否需要修改 | 只读 | 需要写入或修改 |
| 性能要求 | 零拷贝，性能最优 | 需要拷贝，略慢 |
| 生命周期 | 必须手动保证 | 由 SourceBuffer 管理 |
| 内存占用 | 16 字节（引用） | 实际数据大小 |
| 使用难度 | 需要小心生命周期 | 相对安全 |

**决策流程**：

```text
是否需要修改数据？
  ├─ 是 → 使用 StringBuffer
  └─ 否 → 数据来源是什么？
           ├─ SourceBuffer 中 → 使用 StringView（零拷贝）
           ├─ 静态字符串 → 使用 StringView
           ├─ 临时变量 → 先用 StringBuffer 拷贝，再转 StringView
           └─ 不确定生命周期 → 使用 StringBuffer（安全第一）
```

#### 3. SourceBuffer

`SourceBuffer` 是统一的内存分配器，管理事件组内所有字符串数据的内存。

```cpp
class SourceBuffer {
public:
    // 分配指定大小的字符串缓冲区
    StringBuffer AllocateStringBuffer(size_t size);
    
    // 拷贝字符串到 SourceBuffer 管理的内存中
    StringBuffer CopyString(const char* data, size_t len);
    StringBuffer CopyString(const std::string& s);
    StringBuffer CopyString(StringView s);
};
```

**工作原理**：

- 使用 Chunk 机制预分配大块内存（默认 4KB，最大 128KB）
- 小对象在 Chunk 内连续分配，减少碎片
- 大对象（>= Chunk 一半）独立分配，避免浪费
- 所有内存随 `PipelineEventGroup` 一起释放

### 使用模式

所有 Event 类型都提供两套接口：**Copy 模式**和**NoCopy 模式**。

#### Copy 模式

适用于数据来源于临时变量或外部内存的场景。数据会被拷贝到 `SourceBuffer` 中，之后原始数据可以安全释放。

```cpp
// 接受 std::string，内部调用 SourceBuffer::CopyString()
logEvent->SetContent("key", "value");
logEvent->SetContent(std::string("key"), std::string("value"));

metricEvent->SetTag("host", "localhost");
metricEvent->SetName("cpu_usage");

spanEvent->SetTraceId("1234567890abcdef");
```

#### NoCopy 模式（推荐）

适用于数据已经在 `SourceBuffer` 中或生命周期由开发者保证的场景。直接使用指针引用，避免拷贝。

```cpp
// 使用 StringBuffer（数据已在 SourceBuffer 中）
StringBuffer keyBuf = sourceBuffer->CopyString("key");
StringBuffer valBuf = sourceBuffer->CopyString("value");
logEvent->SetContentNoCopy(keyBuf, valBuf);

// 使用 StringView（数据生命周期由开发者保证）
StringView key("key", 3);  // 必须确保 "key" 在 Event 生命周期内有效
StringView val("value", 5);
logEvent->SetContentNoCopy(key, val);

// 混合使用
StringBuffer keyFromBuffer = sourceBuffer->CopyString("metric_name");
metricEvent->SetNameNoCopy(keyFromBuffer);
```

### 最佳实践

#### 场景 1：处理文件数据（推荐 NoCopy）

文件数据已读入 `SourceBuffer`，直接使用 `StringView` 引用：

```cpp
// 文件读取器将数据读入 SourceBuffer
auto sourceBuffer = std::make_shared<SourceBuffer>();
PipelineEventGroup group(sourceBuffer);

// 从文件中解析出的字段，直接引用 SourceBuffer 中的数据
StringView content(fileData.data(), fileData.size());
StringView key = content.substr(0, keyLen);
StringView value = content.substr(keyLen, valueLen);

LogEvent* log = group.AddLogEvent();
log->SetContentNoCopy(key, value);  // 零拷贝
```

#### 场景 2：处理临时字符串（使用 Copy）

数据来自临时变量，需要拷贝保存：

```cpp
LogEvent* log = group.AddLogEvent();

{
    std::string tempKey = GetKeyFromSomewhere();
    std::string tempValue = GetValueFromSomewhere();
    log->SetContent(tempKey, tempValue);  // 拷贝到 SourceBuffer
} // tempKey 和 tempValue 离开作用域被销毁，但数据已安全保存
```

#### 场景 3：字符串拼接（使用 AllocateStringBuffer）

需要构造新字符串时，先分配缓冲区：

```cpp
auto& sourceBuffer = group.GetSourceBuffer();

// 分配缓冲区
StringBuffer buffer = sourceBuffer->AllocateStringBuffer(100);

// 写入数据
int written = snprintf(buffer.data, buffer.capacity, "%s_%s", prefix, suffix);
buffer.size = written;

// 免拷贝设置
logEvent->SetContentNoCopy("composed_key", StringView(buffer.data, buffer.size));
```

#### 场景 4：避免常见错误

```cpp
// ❌ 错误：临时对象的 StringView
{
    std::string temp = "value";
    StringView view(temp);  // view 引用 temp
    logEvent->SetContentNoCopy("key", view);
} // temp 被销毁，view 变成悬空指针

// ✅ 正确：使用 Copy 模式
{
    std::string temp = "value";
    logEvent->SetContent("key", temp);  // 拷贝到 SourceBuffer
}

// ✅ 正确：确保生命周期
const char* staticStr = "value";  // 静态字符串，生命周期永久
logEvent->SetContentNoCopy("key", StringView(staticStr));
```

## Log 模型

`LogEvent` 用于存储日志数据，支持 Key-Value 形式的内容。

```cpp
class LogEvent : public PipelineEvent {
public:
    // Content 操作（KV 对）
    StringView GetContent(StringView key) const;
    bool HasContent(StringView key) const;
    void SetContent(const std::string& key, const std::string& val);  // Copy 模式
    void SetContentNoCopy(StringView key, StringView val);            // NoCopy 模式
    void SetContentNoCopy(const StringBuffer& key, const StringBuffer& val);
    void DelContent(StringView key);
    
    // 位置信息（文件采集时使用）
    void SetPosition(uint64_t offset, uint64_t size);
    std::pair<uint64_t, uint64_t> GetPosition() const;
    
    // 日志级别
    StringView GetLevel() const;
    void SetLevel(const std::string& level);
    
    // 迭代器支持
    ContentIterator begin();
    ContentIterator end();
    size_t Size() const;
    bool Empty() const;
};
```

### 使用示例

```cpp
// 创建 LogEvent
auto sourceBuffer = std::make_shared<SourceBuffer>();
PipelineEventGroup group(sourceBuffer);
LogEvent* log = group.AddLogEvent();

// 设置时间戳
log->SetTimestamp(time(nullptr));

// Copy 模式
log->SetContent("message", "Application started");
log->SetContent("level", "INFO");

// NoCopy 模式
StringBuffer keyBuf = sourceBuffer->CopyString("user_id");
StringBuffer valBuf = sourceBuffer->CopyString("12345");
log->SetContentNoCopy(keyBuf, valBuf);

// 遍历内容
for (const auto& kv : *log) {
    std::cout << kv.first << ": " << kv.second << std::endl;
}
```

## Metric 模型

`MetricEvent` 用于存储时序指标数据。

```cpp
class MetricEvent : public PipelineEvent {
public:
    // 名称
    StringView GetName() const;
    void SetName(const std::string& name);
    void SetNameNoCopy(StringView name);
    
    // 值（支持多种类型）
    template <typename T> bool Is() const;
    template <typename T> const T* GetValue() const;
    template <typename T> T* MutableValue();
    template <typename T> void SetValue(const T& value);
    
    // 标签（Labels）
    StringView GetTag(StringView key) const;
    bool HasTag(StringView key) const;
    void SetTag(const std::string& key, const std::string& val);
    void SetTagNoCopy(StringView key, StringView val);
    void DelTag(StringView key);
    
    // 元数据
    StringView GetMetadata(StringView key) const;
    void SetMetadata(const std::string& key, const std::string& val);
    void SetMetadataNoCopy(StringView key, StringView val);
};
```

### 指标值类型

LoongCollector 支持多种指标值类型：

```cpp
// 单值类型
UntypedSingleValue    // 无类型单值（double）

// 多值类型
UntypedMultiDoubleValues  // 多个 double 值的映射
```

### 使用示例

```cpp
MetricEvent* metric = group.AddMetricEvent();

// 设置名称和时间戳
metric->SetNameNoCopy(sourceBuffer->CopyString("cpu_usage_percent"));
metric->SetTimestamp(time(nullptr));

// 设置单值
metric->SetValue(UntypedSingleValue{85.5});

// 设置标签
metric->SetTag("host", "server01");
metric->SetTag("region", "us-west");

// 设置多值
std::map<StringView, UntypedMultiDoubleValue> values;
values["p50"] = 10.5;
values["p90"] = 25.3;
values["p99"] = 45.8;
metric->SetValue(values);

// 获取值
if (metric->Is<UntypedSingleValue>()) {
    auto* value = metric->GetValue<UntypedSingleValue>();
    std::cout << "Value: " << value->mValue << std::endl;
}
```

## Span 模型

`SpanEvent` 用于存储分布式追踪数据，遵循 OpenTelemetry 规范。

```cpp
class SpanEvent : public PipelineEvent {
public:
    enum class Kind { Unspecified, Internal, Server, Client, Producer, Consumer };
    enum class StatusCode { Unset, Ok, Error };
    
    // 基本信息
    StringView GetTraceId() const;
    void SetTraceId(const std::string& traceId);
    
    StringView GetSpanId() const;
    void SetSpanId(const std::string& spanId);
    
    StringView GetParentSpanId() const;
    void SetParentSpanId(const std::string& parentSpanId);
    
    StringView GetName() const;
    void SetName(const std::string& name);
    
    Kind GetKind() const;
    void SetKind(Kind kind);
    
    // 时间
    uint64_t GetStartTimeNs() const;
    void SetStartTimeNs(uint64_t startTimeNs);
    
    uint64_t GetEndTimeNs() const;
    void SetEndTimeNs(uint64_t endTimeNs);
    
    // 标签
    void SetTag(const std::string& key, const std::string& val);
    void SetTagNoCopy(StringView key, StringView val);
    
    // 事件和链接
    const std::vector<InnerEvent>& GetEvents() const;
    InnerEvent* AddEvent();
    
    const std::vector<SpanLink>& GetLinks() const;
    SpanLink* AddLink();
    
    // 状态
    StatusCode GetStatus() const;
    void SetStatus(StatusCode status);
};
```

### 使用示例

```cpp
SpanEvent* span = group.AddSpanEvent();

// 设置基本信息
span->SetTraceId("1234567890abcdef1234567890abcdef");
span->SetSpanId("1234567890abcdef");
span->SetParentSpanId("fedcba0987654321");
span->SetName("HTTP GET /api/users");
span->SetKind(SpanEvent::Kind::Server);

// 设置时间（纳秒）
span->SetStartTimeNs(1609459200000000000);
span->SetEndTimeNs(1609459200500000000);

// 设置标签
span->SetTag("http.method", "GET");
span->SetTag("http.status_code", "200");

// 添加事件
auto* event = span->AddEvent();
event->SetName("cache_miss");
event->SetTimestampNs(1609459200100000000);

// 设置状态
span->SetStatus(SpanEvent::StatusCode::Ok);
```

## Raw 模型

`RawEvent` 用于存储未经解析的原始数据。

```cpp
class RawEvent : public PipelineEvent {
public:
    StringView GetContent() const;
    void SetContent(const std::string& content);
    void SetContentNoCopy(StringView content);
    void SetContentNoCopy(const StringBuffer& content);
};
```

### 使用示例

```cpp
RawEvent* raw = group.AddRawEvent();

// Copy 模式
raw->SetContent("raw log line content");

// NoCopy 模式（数据已在 SourceBuffer 中）
StringView rawData(buffer.data(), buffer.size());
raw->SetContentNoCopy(rawData);
```

## 性能建议

### 1. 优先使用 NoCopy 模式

在性能敏感的场景（如高吞吐采集），优先使用 NoCopy 模式：

```cpp
// ✅ 推荐：零拷贝
StringBuffer key = sourceBuffer->CopyString("key");
StringBuffer val = sourceBuffer->CopyString("value");
logEvent->SetContentNoCopy(key, val);

// ❌ 不推荐：多次拷贝
logEvent->SetContent(std::string("key"), std::string("value"));
```

### 2. 批量预分配

对于已知数据量的场景，预先分配容器大小：

```cpp
group.ReserveEvents(1000);  // 预分配 1000 个事件的空间
```

### 3. 复用 SourceBuffer

相同来源的数据可共享 `SourceBuffer`：

```cpp
auto sourceBuffer = std::make_shared<SourceBuffer>();
PipelineEventGroup group1(sourceBuffer);
PipelineEventGroup group2(sourceBuffer);  // 复用同一个 SourceBuffer
```

### 4. 避免不必要的字符串转换

```cpp
// ❌ 不推荐：先转 string 再设置
std::string str = view.to_string();
logEvent->SetContent("key", str);

// ✅ 推荐：直接使用 StringView
logEvent->SetContent("key", sourceBuffer->CopyString(view));
```

## 总结

LoongCollector C++ 数据模型通过以下机制实现高性能：

1. **统一内存管理**：`SourceBuffer` 集中管理所有字符串内存，减少碎片和分配开销
2. **零拷贝接口**：通过 `StringView` 和 `StringBuffer` 避免不必要的数据拷贝
3. **灵活的 API 设计**：提供 Copy 和 NoCopy 两套接口，兼顾易用性和性能
4. **类型安全**：编译期类型检查，避免运行时错误

开发者应根据实际场景选择合适的接口：

- 数据来自临时变量 → 使用 **Copy 模式**
- 数据已在 SourceBuffer 或生命周期可控 → 使用 **NoCopy 模式**
