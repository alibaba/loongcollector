# 开源插件开发引导

## 了解 ilogtail 插件

ilogtail插件的实现原理、整体架构、系统设计等介绍，请参考[插件系统](../../principle/plugin-system.md)。

## 开发流程

ilogtail 插件的开发主要有以下步骤：

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 实现相应接口。
3. 通过init函数注册插件。
4. 将插件加入[插件引用配置文件](https://github.com/alibaba/ilogtail/blob/main/plugins.yml)的`common`配置节, 如果仅运行于指定系统，请添加到`linux`或`windows`配置节.
5. 进行单测或者E2E测试，请参考[如何使用单测](../test/unit-test.md) 与 [如何使用E2E测试](../test/e2e-test.md).
6. 使用 *make lint* 检查代码规范。
7. 提交Pull Request。

在开发时，[Checkpoint接口](./checkpoint-api.md)与[Logger接口](./logger-api.md)或许能对您有所帮助。此外，可以使用[纯插件模式启动](./pure-plugin-start.md) iLogtail，用于对插件进行轻量级测试。

更详细的开发细节，请参考：

* [如何开发Input插件](./how-to-write-input-plugins.md)
* [如何开发Processor插件](./how-to-write-processor-plugins.md)
* [如何开发Aggregator插件](./how-to-write-aggregator-plugins.md)
* [如何开发Flusher插件](./how-to-write-flusher-plugins.md)
* [如何开发Extension插件](./how-to-write-extension-plugins.md)
* [插件配置项基本原则](./principles-of-plugin-configuration.md)

## 文档撰写流程

开发完成后，可以参考[如何生成插件文档](./how-to-genernate-plugin-docs.md)生成插件的使用文档，也可以手动编写插件文档。

文档的编写主要有如下步骤：

1. 遵循[插件文档规范](./plugin-doc-templete.md)，编写插件文档。
2. 在[数据流水线概览](https://github.com/Takuka0311/ilogtail/blob/doc/docs/cn/plugins/overview.md)中添加插件的信息。所有的插件按英文名字典序升序排列，添加的时候请注意插入的位置。
3. 在[文档目录](https://github.com/Takuka0311/ilogtail/blob/doc/docs/cn/SUMMARY.md)中添加插件文档的路径，注意与数据流水线概览中保持顺序一致。
