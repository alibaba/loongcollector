# 如何开发原生 Flusher 插件

## 接口定义

基类见 [`core/collection_pipeline/plugin/interface/Flusher.h`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/plugin/interface/Flusher.h)：

```c++
class Flusher : public Plugin {
public:
    virtual bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) = 0;
    virtual bool Start();
    virtual bool Stop(bool isPipelineRemoving);
    virtual bool Send(PipelineEventGroup&& g) = 0;
    virtual bool Flush(size_t key) = 0;
    virtual bool FlushAll() = 0;
    // ...
};
```

注意：`Send` / `Flush` / `FlushAll` 的返回值为 **`bool`**（与旧文档中的 `void` 不同）。

基于 HTTP 发送时可派生 [`HttpFlusher`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/plugin/interface/HttpFlusher.h)：

```c++
class HttpFlusher : public Flusher {
public:
    virtual bool BuildRequest(SenderQueueItem* item, std::unique_ptr<HttpSinkRequest>& req,
                              bool* keepItem, std::string* errMsg) = 0;
    virtual void OnSendDone(const HttpResponse& response, SenderQueueItem* item) = 0;
};
```

## Flusher 级组件

### 聚合（必选）

* **作用**：将多个较小的 `PipelineEventGroup` 按 tag 等规则合并，提高发送效率。
* **配置**：Flusher 参数中的 `Batch` 字段（map），常见子字段：

| 名称 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| MinCnt | uint | 各 Flusher 自定 | 单队列最少 event 数 |
| MinSizeBytes | uint | 各 Flusher 自定 | 单队列最小字节数 |
| TimeoutSecs | uint | 各 Flusher 自定 | 首条 event 入队后最长等待时间 |

* **类接口**（概念说明，以源码为准）：

```c++
template <typename T = EventBatchStatus>
class Batcher {
public:
    bool Init(const Json::Value& config, Flusher* flusher,
              const DefaultFlushStrategyOptions& strategy, bool enableGroupBatch = false);
    void Add(PipelineEventGroup&& g, std::vector<BatchedEventsList>& res);
    void FlushQueue(size_t key, BatchedEventsList& res);
    void FlushAll(std::vector<BatchedEventsList>& res);
};
```

### 序列化（必选）

* **作用**：event 级与 event group 级的序列化。
* **类接口**（概念说明）：

```c++
template <typename T>
class Serializer {
    virtual bool Serialize(T&& p, std::string& res, std::string& errorMsg) = 0;
};
```

### 压缩（可选）

* **类接口**（概念说明）：

```c++
class Compressor {
    virtual bool Compress(const std::string& input, std::string& output, std::string& errorMsg) = 0;
};
```

## 开发步骤（以 HttpFlusher 为例）

1. 在 [`core/plugin/flusher`](https://github.com/alibaba/loongcollector/tree/main/core/plugin/flusher) 下新增 `FlusherXxx.h` / `FlusherXxx.cpp`，派生 `HttpFlusher`。
2. 头文件中：可配置字段多为 **public**；一般包含 `Batcher<>`、`std::unique_ptr` 持有的 **Serializer**、可选 **Compressor**。
3. 在 [`core/pipeline/serializer`](https://github.com/alibaba/loongcollector/tree/main/core/pipeline/serializer)（或现有同类目录）中新增序列化实现（以仓库实际布局为准）。
4. （可选）新增压缩算法：`CompressType`、实现类、并在 `CompressorFactory` 中注册。
5. 在 `FlusherXxx.cpp` 中：
   * **Init**：解析参数并初始化 Batcher / Serializer / Compressor。
   * **SerializeAndPush**：序列化（与可选压缩）、构造 `SenderQueueItem` 并 `PushToQueue`。
   * **Send / Flush / FlushAll**：通过 `mBatcher` 聚合后进入 `SerializeAndPush`。
   * **BuildRequest / OnSendDone**：HTTP 请求构建与回调处理。
6. 在 [`PluginRegistry.cpp`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/plugin/PluginRegistry.cpp) 中加入头文件与  
   `RegisterFlusherCreator(new StaticFlusherCreator<FlusherXxx>());`

具体成员函数签名与 `SerializeAndPush` 重载形式请参考现有 Flusher 实现（如 SLS、Kafka），本文档仅保留流程级说明。
