# Alibaba LoongCollector - Fast and Lightweight Observability Data Collector | 中文用户手册（[GitBook](https://ilogtail.gitbook.io/ilogtail-docs/) | [可观测中文社区](https://open.observability.cn/project/loongcollector/about/)）

<img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/logo/jpg/black-blue.jpg" alt="loongcollector logo" height="100px" align="center" />

LoongCollector was born for observable scenarios and has many production-level features such as lightweight, high performance, and automated configuration, which are widely used internally by Alibaba Group and tens of thousands of external Alibaba Cloud customers. You can deploy it in physical machines, Kubernetes and other environments to collect telemetry data, such as logs, traces and metrics.

[![GitHub contributors](https://img.shields.io/github/contributors/alibaba/ilogtail)](https://github.com/alibaba/loongcollector/contributors)
[![GitHub stars](https://img.shields.io/github/stars/alibaba/ilogtail)](https://github.com/alibaba/loongcollector/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/alibaba/ilogtail)](https://github.com/alibaba/loongcollector/issues)
[![GitHub license](https://img.shields.io/github/license/alibaba/ilogtail)](https://github.com/alibaba/loongcollector/blob/main/LICENSE)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/28764.svg)](https://scan.coverity.com/projects/alibaba-ilogtail)
[![Coverage Status](https://codecov.io/gh/alibaba/ilogtail/branch/main/graph/badge.svg)](https://codecov.io/gh/alibaba/ilogtail)
[![Go Report Card](https://goreportcard.com/badge/github.com/alibaba/loongcollector)](https://goreportcard.com/report/github.com/alibaba/loongcollector)

## Abstract

The core advantages of **LoongCollector**:

* Support a variety of Logs, Traces, Metrics data collection, and friendly to container and Kubernetes environment support.
* The resource cost of data collection is quite low, 5-20 times better than similar telemetry data collection Agent performance.
* High stability, used in the production of Alibaba and tens of thousands of Alibaba Cloud customers,  and collecting dozens of petabytes of observable data every day with nearly tens of millions deployments.
* Support plugin expansion, such as collection, processing, aggregation, and sending modules.
* Support configuration remote management and provide a variety of ways, such as SLS console, SDK, K8s Operator, etc.
* Supports multiple advanced features such as self-monitoring, flow control, resource control, alarms, and statistics collection.

**LoongCollector** supports the collection of a variety of telemetry data and transmission to a variety of different backends, such as [SLS observable platform](https://www.aliyun.com/product/sls). The data supported for collection are mainly as follows:

* Logs
  * Collect static log files
  * Dynamic collect the files when running with containerized environment
  * Dynamic collect Stdout when running with containerized environment
* Traces
  * OpenTelemetry protocol
  * Skywalking V2 protocol
  * Skywalking V3 protocol
  * ...
* Metrics
  * Node metrics
  * Process metrics
  * Gpu metrics
  * Nginx metrics
  * Support fetch prometheus metrics
  * Support transfer telegraf metrics
  * ...

## Quick Start

For the complexity of C++ dependencies, the compilation of LoongCollector requires you have docker installed. If you aim to build LoongCollector from sources, you can go ahead and start with the following commands.

1. Start with local

```bash
make
cp -r example_config/quick_start/* output
cd output
./loongcollector
# Now, LoongCollector is collecting data from output/simple.log and outputing the result to stdout
```

 HEAD

## Documentation

Our official **User Manual** is located here:

[Homepage](https://ilogtail.gitbook.io/ilogtail-docs/about/readme)

[Download](https://ilogtail.gitbook.io/ilogtail-docs/installation/release-notes)

[Installation](https://ilogtail.gitbook.io/ilogtail-docs/installation/quick-start)

[Configuration](https://ilogtail.gitbook.io/ilogtail-docs/configuration/collection-config)

[All Plugins](https://ilogtail.gitbook.io/ilogtail-docs/plugins/overview)

[Getting Started](https://ilogtail.gitbook.io/ilogtail-docs/awesome-ilogtail/getting-started)

[Developer Guide](https://ilogtail.gitbook.io/ilogtail-docs/developer-guide/)

[Benchmark](https://ilogtail.gitbook.io/ilogtail-docs/benchmark/)

## Contribution

There are many ways to contribute:

* [Fix and report bugs](https://github.com/alibaba/loongcollector/issues)
* [Improve Documentation](https://github.com/alibaba/loongcollector/labels/documentation)
* [Review code and feature proposals](https://github.com/alibaba/loongcollector/pulls)
* [Contribute plugins](./docs/en/guides/README.md)

## Contact Us

You can report bugs, make suggestions or participate in discussions through [Github Issues](https://github.com/alibaba/loongcollector/issues) and [Github Discussions](https://github.com/alibaba/loongcollector/discussions), or contact us with the following ways:

* Bilibili：[阿里云SLS](https://space.bilibili.com/630680534)
* Zhihu：[LoongCollector社区](https://www.zhihu.com/column/c_1533139823409270785)
* DingTalk/WeChat：LoongCollector社区

<img src="docs/cn/.gitbook/assets/chatgroup.png" style="width: 60%; height: 60%" />

## Our Users

Tens of thousands of companies use LoongCollector in Alibaba Cloud, IDC, or other clouds. More details please see [here](https://help.aliyun.com/document_detail/250269.html).

## Licence

[Apache 2.0 License](./LICENSE)
