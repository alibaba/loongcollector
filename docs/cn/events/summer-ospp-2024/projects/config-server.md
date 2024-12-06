# ConfigServer 能力升级 + 体验优化（全栈）

## 项目链接

<https://summer-ospp.ac.cn/org/prodetail/246eb0236?lang=zh&list=pro>

## 项目描述

### iLogtail介绍

iLogtail 为可观测场景而生，拥有轻量级、高性能、自动化配置等诸多生产级别特性，在阿里巴巴以及外部数万家阿里云客户内部广泛应用。你可以将它部署于物理机，虚拟机，Kubernetes等多种环境中来采集可观测数据，例如logs、traces和metrics。目前来自阿里云、石墨文档、同程旅行、小红书、字节跳动、哔哩哔哩、嘀嗒出行的多位同学在参与 iLogtail 社区的共建。iLogtail 代码库：<https://github.com/alibaba/loongcollector>

### ConfigServer介绍

目前可观测采集 Agent，例如 iLogtail 开源版，主要提供了本地采集配置管理模式，当涉及实例数较多时，需要逐个实例进行配置变更，管理比较复杂。此外，多实例场景下 Agent 的版本信息、运行状态等也缺乏统一的监控。为了解决这个问题，我们有了 ConfigServer 项目。
ConfigServer 是一款用于管控可观测采集 Agent 的工具，目前支持的功能有：

* 采集 Agent 注册到 ConfigServer
* 以 Agent 组的形式对采集 Agent 进行统一管理
* 远程批量管理采集 Agent 的采集配置
* 监控采集 Agent 的运行状态
* 管控UI

### 项目具体要求

ConfigServer 当前的部署不够灵活、管控能力不够全面，需要优化使用体验。本项目需要进行全栈开发，实现以下需求：

* 管控能力升级：支持多 Agent 组管控；支持 Agent 上报告警。
* 部署能力升级：支持容器化部署、开箱即用；支持分布式部署。
* 管控体验优化：前端适配多Agent组等新功能；前端支持采集 Agent 监控与自监控。

## 项目导师

玄飏（<yangzehua.yzh@alibaba-inc.com>）

## 项目难度

进阶

## 技术领域标签

React,Mysql,Redis,Docker

## 编程语言标签

Go,JavaScript,C++

## 项目产出要求

1. 管控能力升级
    1. ConfigServer 支持多 Agent 组管控。
    2. ConfigServer 支持 Agent 上报告警信息。
2. 部署能力升级
    1. 基于 ConfigServer 预留的数据存储层接口，实现分布式部署。
    2. 支持 ConfigServer 容器化部署，做到开箱即用。
3. 体验优化
    1. ConfigServer UI升级，支持多 Agent 组、Agent 告警查看等新增功能
    2. ConfigServer 提供监控能力，包括监控 ConfigServer 自身状态、监控 Agent 状态，并在UI界面展示。
4. 编写 ConfigServer 使用与开发文档

## 项目技术要求

1. 掌握 Golang，熟悉 React 或其他前端开发框架，了解C++
2. 熟悉分布式应用开发
3. 熟悉容器化技术
4. 熟悉Git、GitHub相关操作
5. 有阿里云日志服务使用经验更优

## 参考资料

ConfigServer介绍：<https://ilogtail.gitbook.io/ilogtail-docs/config-server/quick-start>
