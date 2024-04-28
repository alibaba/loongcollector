# iLogtail 数据吞吐性能优化

## 项目链接

<https://summer-ospp.ac.cn/org/prodetail/246eb0231?lang=zh&list=pro>

## 项目描述

### iLogtail介绍

iLogtail 为可观测场景而生，拥有轻量级、高性能、自动化配置等诸多生产级别特性，在阿里巴巴以及外部数万家阿里云客户内部广泛应用。你可以将它部署于物理机，虚拟机，Kubernetes等多种环境中来采集可观测数据，例如logs、traces和metrics。目前来自阿里云、石墨文档、同程旅行、小红书、字节跳动、哔哩哔哩、嘀嗒出行的多位同学在参与 iLogtail 社区的共建。iLogtail 代码库：<https://github.com/alibaba/ilogtail>

### iLogtail 数据接入介绍

iLogtail作为阿里云SLS提供的可观测数据采集器，可以运行在服务器、容器、K8s、嵌入式等多种环境，支持采集数百种可观测数据（日志、监控、Trace、事件等），已经有千万级的安装量。为了能够更好地实现与开源生态的融合并满足用户不断丰富的接入需求，我们为 iLogtail 引入了 Golang 实现的数据接入系统，目前，我们已通过该系统支持了若干数据源，包括 Kafka、OTLP、HTTP、MySQL Query、MySQL Binlog 等。

### 项目具体要求

Golang 实现的数据接入、处理、发送能力，效率上还是与 C++ 有差距。在 iLogtail 2.0 中，我们实现了 C++ 版本的数据处理流水线，希望将一些高频的数据源输出、解析、输出模块，升级到 C++ 版本，来提升 iLogtail 在这些场景的输入输出速率。

本次项目，我们旨在实现 Syslog 输入、Nginx 解析、Kafka 输出等高频使用模块的更高性能的 C++ 版本，来提升 iLogtail 的数据吞吐能力。升级后的版本需要满足下面的条件：

* 性能更优：
  * Syslog 输入、Kafka 输出，相较于升级前的 Golang 版本，支持更高的读取/发送速率
  * Nginx 解析速率需要高于 C++ 正则解析
* 内存优化：相同处理速率时，内存开销小于升级前的 Golang 版本
* 数据Tag管理：iLogtail内部的tag与输入/输出源的tag格式不一致，需要能够管理好tag间的转换。
* 数据完整性：满足“at least once”原则，保证不丢失数据

## 项目导师

阿柄（<bingchang.cbc@alibaba-inc.com>）

## 项目难度

进阶

## 技术领域标签

Kafka,TCP/IP,DataOps

## 编程语言标签

C++,Go

## 项目产出要求

1. 实现 C++ 版本的 Syslog 输入、Nginx 解析、Kafka 输出模块
2. 为实现的功能模块提供完善的说明文档和使用案例
3. 为实现的功能模块添加完善的测试
4. 编写实现的功能模块与 Golang 版本的性能对比测试报告

## 项目技术要求

1. 掌握C++，了解Golang
2. 了解 Kafka 客户端 API 和集群架构
3. 了解 syslog 协议及相关库函数
4. 了解 Git、GitHub 相关操作
5. 有开发高性能网络服务/插件经验更优

## 参考资料

什么是iLogtail：<https://ilogtail.gitbook.io/ilogtail-docs>

你好，iLogtail 2.0！：<https://developer.aliyun.com/article/1441630>
