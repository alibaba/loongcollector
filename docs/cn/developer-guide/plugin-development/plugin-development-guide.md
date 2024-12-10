# 开源插件开发引导

## 了解 LoongCollector 插件

LoongCollector 插件的实现原理、整体架构、系统设计等介绍，请参考[插件系统](../../principle/plugin-system.md)。

## 原生插件开发流程（C++语言）

LoongCollector 原生插件的开发主要有以下步骤：

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 开发对应插件，可以参考以下文档：
    * [如何开发原生Input插件](native-plugins/how-to-write-native-input-plugins.md)
    * [如何开发原生Flusher插件](native-plugins/how-to-write-native-flusher-plugins.md)
    * [插件配置项基本原则](extended-plugins/principles-of-plugin-configuration.md)
3. 进行单测或者E2E测试，请参考[如何使用单测](../test/unit-test.md) 与 [如何使用E2E测试](../test/e2e-test.md).
4. 提交Pull Request。

## 扩展插件开发流程（go语言）

LoongCollector 插件的开发主要有以下步骤：

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 实现相应接口。
3. 通过init函数注册插件。
4. 将插件加入[插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml)的`common`配置节, 如果仅运行于指定系统，请添加到`linux`或`windows`配置节.
5. 进行单测或者E2E测试，请参考[如何使用单测](../test/unit-test.md) 与 [如何使用E2E测试](../test/e2e-test.md).
6. 使用 *make lint* 检查代码规范。
7. 提交Pull Request。

在开发时，[Logger接口](plugin-debug/logger-api.md)和[自监控指标接口](plugin-debug/plugin-self-monitor-guide.md)或许能对您有所帮助。此外，可以使用[纯插件模式启动](plugin-debug/pure-plugin-start.md) LoongCollector，用于对插件进行轻量级测试。

更详细的开发细节，请参考：

* [如何开发扩展Input插件](extended-plugins/how-to-write-input-plugins.md)
* [如何开发扩展Processor插件](extended-plugins/how-to-write-processor-plugins.md)
* [如何开发扩展Aggregator插件](extended-plugins/how-to-write-aggregator-plugins.md)
* [如何开发扩展Flusher插件](extended-plugins/how-to-write-flusher-plugins.md)
* [如何开发扩展Extension插件](extended-plugins/how-to-write-extension-plugins.md)
* [插件配置项基本原则](extended-plugins/principles-of-plugin-configuration.md)
* [如何开发外部私有插件](extended-plugins/how-to-write-external-plugins.md)
* [如何自定义构建产物中默认包含的插件](extended-plugins/how-to-custom-builtin-plugins.md)

## 文档撰写流程

开发完成后，可以参考[如何生成插件文档](plugin-docs/how-to-genernate-plugin-docs.md)生成插件的使用文档，也可以手动编写插件文档。

文档的编写主要有如下步骤：

1. 遵循[插件文档规范](plugin-docs/plugin-doc-templete.md)，编写插件文档。
2. 在[数据流水线概览](https://github.com/alibaba/loongcollector/blob/main/docs/cn/plugins/overview.md)中添加插件的信息。所有的插件按英文名字典序升序排列，添加的时候请注意插入的位置。
3. 在[文档目录](https://github.com/alibaba/loongcollector/blob/main/docs/cn/SUMMARY.md)中添加插件文档的路径，注意与数据流水线概览中保持顺序一致。
