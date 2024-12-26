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
    RegisterInputCreator(new StaticInputCreator<Inputxxx>());
    ```

   c. 在`PipelineManager`类的构造函数中注册该插件的管理类
