# 如何开发原生Flusher插件

## 接口定义

```c++
class Flusher : public Plugin {
public:
    // 用于初始化插件参数，同时根据参数初始化Flusher级的组件
    virtual bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) = 0;
    virtual bool Start();
    virtual bool Stop(bool isPipelineRemoving);
    // 用于将处理插件的输出经过聚合、序列化和压缩处理后，放入发送队列
    virtual void Send(PipelineEventGroup&& g) = 0;
    // 用于将聚合组件内指定聚合队列内的数据进行强制发送
    virtual void Flush(size_t key) = 0;
    // 用于将聚合组件内的所有数据进行强制发送
    virtual void FlushAll() = 0;
};
```

对于使用Http协议发送数据的Flusher，进一步定义了HttpFlusher，接口如下：

```c++
class HttpFlusher : public Flusher {
public:
    // 用于将待发送数据打包成http请求
    virtual bool BuildRequest(SenderQueueItem* item, std::unique_ptr<HttpSinkRequest>& req, bool* keepItem) = 0;
    // 用于发送完成后进行记录和处理
    virtual void OnSendDone(const HttpResponse& response, SenderQueueItem* item) = 0;
};
```

## Flusher级组件

### 聚合（必选）

* 作用：将多个小的PipelineEventGroup根据tag异同合并成一个大的group，提升发送效率

* 参数：

可以在flusher的参数中配置Batch字段，该字段的类型为map，其中允许包含的字段如下：

|  **名称**  |  **类型**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- |
|  MinCnt  |  uint  |  每个Flusher自定义  |  每个聚合队列最少包含的event数量  |
|  MinSizeBytes  |  uint  |  每个Flusher自定义  |  每个聚合队列最小的尺寸  |
|  TimeoutSecs  |  uint  |  每个Flusher自定义  |  每个聚合队列在第一个event加入后，在被输出前最多等待的时间  |

* 类接口：

```c++
template <typename T = EventBatchStatus>
class Batcher {
public:
    bool Init(const Json::Value& config,
              Flusher* flusher,
              const DefaultFlushStrategyOptions& strategy,
              bool enableGroupBatch = false);
    void Add(PipelineEventGroup&& g, std::vector<BatchedEventsList>& res);
    void FlushQueue(size_t key, BatchedEventsList& res);
    void FlushAll(std::vector<BatchedEventsList>& res);
}
```

### 序列化（必选）

* 作用：对聚合模块的输出进行序列化，分为2个层级：

  * event级：对每一个event单独进行序列化

  * event group级：对多个event进行批量的序列化

* 类接口：

```c++
// T: PipelineEventPtr, BatchedEvents, BatchedEventsList
template <typename T>
class Serializer {
private:
    virtual bool Serialize(T&& p, std::string& res, std::string& errorMsg) = 0;
};

using EventSerializer = Serializer<PipelineEventPtr>;
using EventGroupSerializer = Serializer<BatchedEvents>;
```

### 压缩（可选）

* 作用：对序列化后的结果进行压缩

* 类接口：

```c++
class Compressor {
private:
    virtual bool Compress(const std::string& input, std::string& output, std::string& errorMsg) = 0;
};
```

## 开发步骤

下面以开发一个HttpFlusher为例，说明整个开发步骤：

1. 在plugin/flusher目录下新建一个Flusherxxx.h和Flusherxxx.cpp文件，用于派生HttpFlusher接口生成具体的插件类；

2. 在Flusherxxx.h文件中定义新的输出插件类Flusherxxx，满足以下要求：

   a. 所有的可配置参数的权限为public，其余参数的权限均为private

   b. 新增一个聚合组件：`Batcher<> mBatcher;`

   c. 新增一个序列化组件：`std::unique_ptr<T> mSerializer;`，其中T为`EventSerializer`和`EventGroupSerializer`中的一种

   d. 如果需要压缩，则新增一个压缩组件：`std::unique_ptr<Compressor> mCompressor;`

3. 在pipeline/serializer目录下新建一个xxxSerializer.h和xxxSerializer.cpp文件，用于派生`Serializer` 接口生成具体类；

4. （可选）如果需要压缩组件，且现有压缩组件库中没有所需算法，则新增一个压缩组件：

    a. 在common/compression/CompressType.h文件中，扩展CompressType类用以标识新的压缩算法；

    b. 在common/compression目录下新建一个xxxCompressor.h和xxxCompressor.cpp文件，用于派生`Compressor`接口生成具体类；

    c. 在common/compression/CompressorFactory.cpp文件的各个函数中注册该压缩组件；

5. 在Flusherxxx.cpp文件中实现插件类

    a. `Init`函数：

       i. 根据入参初始化插件，针对非法参数，根据非法程度和影响决定是跳过该参数、使用默认值或直接拒绝加载插件。

       ii. 调用相关函数完成聚合、序列化和压缩组件的初始化

    b. `SerializeAndPush（BatchedEventsList&&）`函数：

    ```c++
    void Flusherxxx::SerializeAndPush(BatchedEventsList&& groupLists) {
        // 1. 使用mSerializer->Serialize函数对入参序列化
        // 2. 如果需要压缩，则使用mCompressor->Compress函数对序列化结果进行压缩
        // 3. 构建发送队列元素，其中，
        //   a. data为待发送内容
        //   b. 如果没用压缩组件，则rawSize=data.size();否则，rawSize为压缩前（序列化后）数据的长度
        //   c. mLogstoreKey为发送队列的key
        auto item = make_unique<SenderQueueItem>(std::move(data),
                                                rawSize,
                                                this,
                                                mQueueKey);
        Flusher::PushToQueue(std::move(item));
    }
    ```

    c. `SerializeAndPush（vector<BatchedEventsList>&&）`函数：

        ```c++
        void Flusherxxx::SerializeAndPush(vector<BatchedEventsList>&& groupLists) {
            for (auto& groupList : groupLists) {
                SerializeAndPush(std::move(groupList));
            }
        }
        ```

    d. `Send`函数：

        ```c++
        void Flusherxxx::Send(PipelineEventGroup&& g) {
            vector<BatchedEventsList> res;
            mBatcher.Add(std::move(g), res);
            SerializeAndPush(std::move(res));
        }
        ```

    e. `Flush`函数：

        ```c++
        void Flusherxxx::Flush(size_t key) {
            BatchedEventsList res;
            mBatcher.FlushQueue(key, res);
            SerializeAndPush(std::move(res));
        }
        ```

    f. `FlushAll`函数：

        ```c++
        void Flusherxxx::FlushAll() {
            vector<BatchedEventsList> res;
            mBatcher.FlushAll(res);
            SerializeAndPush(std::move(res));
        }

        ```

    g. `BuildRequest`函数：将待发送数据包装成一个Http请求，如果请求构建失败，使用`keepItem`参数记录是否要保留数据供以后重试。

    h. `OnSendDone`函数：根据返回的http response进行相应的记录和操作。

6. 在`PluginRegistry`类中注册该插件：

7. 在pipeline/plugin/PluginRegistry.cpp文件的头文件包含区新增如下行：

    ```c++
    #include "plugin/flusher/Flusherxxx.h"
    ```

8. 在`PluginRegistry`类的`LoadStaticPlugins()`函数中新增如下行：

    ```c++
    RegisterFlusherCreator(new StaticFlusherCreator<Flusherxxx>());
    ```
