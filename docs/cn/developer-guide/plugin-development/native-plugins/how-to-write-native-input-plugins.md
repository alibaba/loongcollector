# 如何开发原生 Input 插件

## 工作模式

同一输入类型的多个插件实例通常由**管理类（Runner）**在共享线程或任务模型下驱动；实例侧保存配置并参与注册/反注册。

## 接口定义（C++）

原生 Input 声明见 [`core/collection_pipeline/plugin/interface/Input.h`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/plugin/interface/Input.h)（派生自 `Plugin`）：

```c++
class Input : public Plugin {
public:
    virtual bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) = 0;
    virtual bool Start() = 0;
    virtual bool Stop(bool isPipelineRemoving) = 0;
    virtual QueueType GetProcessQueueType() const = 0;
    // ...
};
```

与早期文档相比，请注意 **`Init` 增加 `optionalGoPipeline` 参数**，且必须实现 **`GetProcessQueueType`**。

## 开发步骤

1. 在 [`core/plugin/input`](https://github.com/alibaba/loongcollector/tree/main/core/plugin/input) 下新增 `InputXxx.h` / `InputXxx.cpp`，派生 `Input` 实现具体插件。

2. 在头文件中定义插件类：可配置字段一般为 **public**，其余成员 **private**。

3. 在 `Init` 中解析配置；非法参数按影响程度选择跳过、默认值或拒绝加载。

4. 在独立目录中实现**管理类 / Runner**（负责线程、调度与多实例注册），管理类应继承 [`core/runner/InputRunner.h`](https://github.com/alibaba/loongcollector/blob/main/core/runner/InputRunner.h) 中的 **`InputRunner`**：

```c++
class InputRunner {
public:
    // 由插件 Start 路径触发：初始化并至少启动一条采集路径；需可重入（已启动则直接返回）
    virtual void Init() = 0;
    // 进程退出或配置卸载后无实例时：停止并回收
    virtual void Stop() = 0;
    // 热加载结束后由框架调用：是否仍有插件注册，无则 Stop
    virtual bool HasRegisteredPlugins() const = 0;
};
```

`InputRunner` 的构造函数会向 [`CollectionPipelineManager`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/CollectionPipelineManager.h) **自动注册**自身（见 `InputRunner.cpp`）。若自定义 Runner 不继承 `InputRunner`，需在合适时机调用 `CollectionPipelineManager::GetInstance()->RegisterInputRunner(...)`。

管理类基本流程：

* 遍历已注册的各实例配置并采集数据；
* 将数据转为 `PipelineEvent` / `PipelineEventGroup`；
* 投递到处理队列，例如：

```c++
ProcessorRunner::GetInstance()->PushQueue(queueKey, inputIndex, std::move(group));
```

其中：

* **queueKey**：处理队列键，可由**采集配置**上下文（如 [`CollectionPipelineContext::GetProcessQueueKey()`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/CollectionPipelineContext.h)）取得。
* **inputIndex**：该 Input 在**当前采集配置** **`inputs` 列表**中的下标（从 0 开始），与 `Input::SetInputIndex` 一致。
* **group**：待发送的 `PipelineEventGroup`。

注册/注销应**尽量独立于**采集主循环，避免单实例变更阻塞全体线程。

5. 在 `InputXxx.cpp` 中实现 `Start` / `Stop`：`Start` 中启动或挂靠管理类并注册本实例；`Stop` 中从管理类注销。

6. 在 **PluginRegistry** 中注册插件：

   * 在 [`core/collection_pipeline/plugin/PluginRegistry.cpp`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/plugin/PluginRegistry.cpp) 头部增加 `#include "plugin/input/InputXxx.h"`（路径以工程 include 规则为准）。
   * 在 `LoadStaticPlugins()` 中增加：`RegisterInputCreator(new StaticInputCreator<InputXxx>(), false);`

7. 其余与**采集配置生命周期**、队列、检查点交互的细节，请对标现有原生 Input（如 `InputFile`、`InputPrometheus`）的阅读路径按需补齐。
