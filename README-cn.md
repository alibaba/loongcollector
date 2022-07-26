Alibaba iLogtail - 高性能轻量级可观测性数据采集器 | [English](./README.md)
==========
<img src="https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/ilogtail.svg?versionId=CAEQMxiBgIDEmq.m6BciIDkzNmE2OWU4NzIwZjQ1Y2ZiYmIxZjhiYjMyNmQxZTdi" alt="ilogtail logo" height="150px" align="right" />

iLogtail 为可观测场景而生，拥有的轻量级、高性能、自动化配置等诸多生产级别特性，在阿里巴巴以及外部数万家阿里云客户内部广泛应用。你可以将它部署于物理机，虚拟机，Kubernetes等多种环境中来采集遥测数据，例如logs、traces和metrics。

[![GitHub stars](https://img.shields.io/github/stars/alibaba/ilogtail)](https://github.com/alibaba/ilogtail/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/alibaba/ilogtail)](https://github.com/alibaba/ilogtail/issues)
[![GitHub license](https://img.shields.io/github/license/alibaba/ilogtail)](https://github.com/alibaba/ilogtail/blob/main/LICENSE)

**iLogtail** 的核心优势主要有：
* 支持多种Logs、Traces、Metrics数据采集，尤其对容器、Kubernetes环境支持非常友好
* 数据采集资源消耗极低，相比同类遥测数据采集的Agent性能好5-20倍
* 高稳定性，在阿里巴巴以及数万阿里云客户生产中使用验证，部署量近千万，每天采集数十PB可观测数据
* 支持插件化扩展，可任意扩充数据采集、处理、聚合、发送模块
* 支持配置远程管理，支持以图形化、SDK、K8s Operator等方式进行配置管理，可轻松管理百万台机器的数据采集
* 支持自监控、流量控制、资源控制、主动告警、采集统计等多种高级特性

**iLogtail** 支持收集多种遥测数据并将其传输到多种不同的后端，例如 [SLS可观测平台](https://www.aliyun.com/product/sls) 。 支持采集的数据主要如下:
- Logs
    - 收集静态日志文件
    - 在容器化环境中运行时动态收集文件
    - 在容器化环境中运行时动态收集 Stdout
- Traces
    - OpenTelemetry 协议
    - Skywalking V2 协议
    - Skywalking V3 协议
    - ...
- Metrics
    - Node指标
    - Process指标
    - GPU 指标
    - Nginx 指标
    - 支持获取Prometheus指标
    - 支持收集Telegraf指标
    - ...

# 快速开始
由于C++编译环境较为复杂，iLogtail的编译依赖docker。如果想从源码编译iLogtail，可以执行下面的命令：
``` bash
make
cp example/quick_start/* output
cd output
./ilogtail
# 现在ilogtail已经开始采集output/simple.log文件并输出到标准输出了
```
如果你对细节感兴趣，请参见文档[编译](https://ilogtail.gitbook.io/ilogtail-docs/installation/sources/build)。
# 文档
**官方用户手册**地址如下：

* [文档首页](https://ilogtail.gitbook.io/ilogtail-docs/about/readme)

* [下载](https://ilogtail.gitbook.io/ilogtail-docs/installation/release-notes)

* [安装](https://ilogtail.gitbook.io/ilogtail-docs/installation/quick-start)

* [配置](https://ilogtail.gitbook.io/ilogtail-docs/configuration/)

* [所有插件](https://ilogtail.gitbook.io/ilogtail-docs/data-pipeline/overview)

* [使用入门](https://ilogtail.gitbook.io/ilogtail-docs/getting-started/)

* [开发指南](https://ilogtail.gitbook.io/ilogtail-docs/developer-guide/)

* [性能测试](https://ilogtail.gitbook.io/ilogtail-docs/benchmark/)

# 贡献

- [修复和报告错误](https://github.com/alibaba/ilogtail/issues)
- [改进文档](https://github.com/alibaba/ilogtail/labels/documentation)
- [审查代码和功能提案](https://github.com/alibaba/ilogtail/pulls)
- [贡献插件](./docs/zh/guides/README.md)

# 联系我们
您可以通过[Github Issues](https://github.com/alibaba/ilogtail/issues) 报告bug、提出建议或参与讨论，或通过以下方式联系我们：

- 钉钉：iLogtail社区
- 微信：iLogtail社区
- 哔哩哔哩：[阿里云SLS](https://space.bilibili.com/630680534?from=search&seid=2845737427240690794&spm_id_from=333.337.0.0)
- 知乎：[阿里云日志服务](https://www.zhihu.com/people/a-li-yun-ri-zhi-fu-wu)

<img src="https://github.com/iLogtail/ilogtail-docs/raw/main/.gitbook/assets/chatgroup.png" style="width: 100%; height: 100%" />

# 我们的用户

数以万计的公司在阿里云、线下IDC、其他云等多种环境中使用 iLogtail。 更多详情请看[这里](https://help.aliyun.com/document_detail/250269.html) 。

# Licence
[Apache 2.0 许可证](./LICENSE)
