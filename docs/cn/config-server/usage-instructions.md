# 使用介绍

## 简介

目前开源版 LoongCollector 主要提供了本地采集配置管理模式，当涉及实例数较多时，需要逐个实例进行配置变更，管理比较复杂。

此外，多实例场景下 LoongCollector 的版本信息、运行状态等也缺乏统一的监控。因此，需要提供全局管控服务用于对 LoongCollector 的采集配置、版本信息、运行状态等进行统一的管理。

ConfigServer 就是这样的一款管控工具，目前支持：

* LoongCollector 注册到 ConfigServer
* 以 Agent 组的形式对 LoongCollector 进行统一管理
* 远程批量配置 LoongCollector 的采集配置
* 监控 LoongCollector 的运行状态

代码库与使用方法参见[ConfigServer](https://github.com/iLogtail/ConfigServer)。
