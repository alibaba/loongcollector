Alibaba iLogtail - The Lightweight Collector of SLS in Alibaba Cloud | [中文版本](https://ilogtail.gitbook.io/ilogtail-docs/about/readme)
==========
<img src="https://github.com/iLogtail/ilogtail-docs/raw/main/.gitbook/assets/ilogtail-icon.png" style="width: 100%; height: 100%" />

iLogtail was born for observable scenarios and has many production-level features such as lightweight, high performance, and automated configuration, which are widely used internally by Alibaba Group and tens of thousands of external Alibaba Cloud customers. You can deploy it in physical machines, Kubernetes and other environments to collect telemetry data, such as logs, traces and metrics.

[![GitHub stars](https://img.shields.io/github/stars/alibaba/ilogtail)](https://github.com/alibaba/ilogtail/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/alibaba/ilogtail)](https://github.com/alibaba/ilogtail/issues)
[![GitHub license](https://img.shields.io/github/license/alibaba/ilogtail)](https://github.com/alibaba/ilogtail/blob/main/LICENSE)

# Abstract

The core advantages of **iLogtail** :
* Support a variety of Logs, Traces, Metrics data collection, and friendly to container and Kubernetes environment support.
* The resource cost of data collection is quite low, 5-20 times better than similar telemetry data collection Agent performance.
* High stability, used in the production of Alibaba and tens of thousands of Alibaba Cloud customers,  and collecting dozens of petabytes of observable data every day with nearly tens of millions deployments.
* Support plugin expansion, such as collection, processing, aggregation, and sending modules.
* Support configuration remote management and provide a variety of ways, such as SLS console, SDK, K8s Operator, etc.
* Supports multiple advanced features such as self-monitoring, flow control, resource control, alarms, and statistics collection.


**iLogtail** supports the collection of a variety of telemetry data and transmission to a variety of different backends, such as [SLS observable platform](https://www.aliyun.com/product/sls). The data supported for collection are mainly as follows:
- Logs
  - Collect static log files
  - Dynamic collect the files when running with containerized environment
  - Dynamic collect Stdout when running with containerized environment
- Traces
  - OpenTelemetry protocol
  - Skywalking V2 protocol
  - Skywalking V3 protocol
  - ...
- Metrics
  - Node metrics
  - Process metrics
  - Gpu metrics
  - Nginx metrics
  - Support fetch prometheus metrics
  - Support transfer telegraf metrics
  - ...

# Quick Start
This repository is the golang part of **iLogtail**，it contains most of the features of iLogtail. And, it can work by itself or work with Ilogtail-C(open source soon) using CGO.

1. Start with local

```shell
make pluin_main && sh output/ilogtail
```

> **NOTE**: for some higher linux version, you have to install systemd-devel in advance
> ```shell
> #centos
> yum install systemd-devel
> 
> #ubuntu
> apt-get update && apt-get install -y libsystemd-dev
> ```
2. Start with Alibaba Cloud  
Please read this [doc](https://www.alibabacloud.com/help/doc-detail/28979.htm).

# Documentation

For documentation on the latest version see the [documentation index](./docs/en/README.md)

- [Input Plugins](./docs/en/guides/How-to-write-input-plugins.md)
- [Processor Plugins](./docs/en/guides/How-to-write-processor-plugins.md)
- [Aggregator Plugins](./docs/en/guides/How-to-write-aggregator-plugins.md)
- [Flusher Plugins](./docs/en/guides/How-to-write-flusher-plugins.md)

# Contribution

There are many ways to contribute:
- [Fix and report bugs](https://github.com/alibaba/ilogtail/issues)
- [Improve Documentation](https://github.com/alibaba/ilogtail/labels/documentation)
- [Review code and feature proposals](https://github.com/alibaba/ilogtail/pulls)
- [Contribute plugins](./docs/en/guides/README.md)

# Contact Us
You can report bugs, make suggestions or participate in discussions through [Github Issues](https://github.com/alibaba/ilogtail/issues), or contact us with the following ways:

- DingTalk：iLogtail社区
- WeChat：日志服务
- Bilibili：[阿里云SLS](https://space.bilibili.com/630680534?from=search&seid=2845737427240690794&spm_id_from=333.337.0.0)
- Zhihu：[阿里云日志服务](https://www.zhihu.com/people/a-li-yun-ri-zhi-fu-wu)

<img src="https://github.com/iLogtail/ilogtail-docs/raw/main/.gitbook/assets/chatgroup.png" style="width: 100%; height: 100%" />



# Our Users
Tens of thousands of companies use iLogtail in Alibaba Cloud, IDC, or other clouds. More details please see [here](https://help.aliyun.com/document_detail/250269.html).

# Licence
[Apache 2.0 License](./LICENSE)
