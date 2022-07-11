Alibaba iLogtail - 阿里轻量级遥测数据采集端 | [English](./README.md)
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

**iLogtail** 支持收集多种遥测数据并将其传输到多种不同的后端，例如 [SLS可观测平台](https://help.aliyun.com/product/28958.html) 。 支持采集的数据主要如下:
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

本仓库是**iLogtail**的开源版本，包括C++核心部分和golang插件部分，含盖了iLogtail的绝大部分功能。承接先前开源的golang部分，保留了纯插件的工作模式。

1.完整模式本地启动

```shell
# Build ilogtail and plugins
make all
# Let's start with a simple config
cp -a example_config/quick_start/* output
# Start ilogtail
cd output
nohup ./ilogtail > stdout.log 2> stderr.log &
# Generate a log
echo 'hello world!' >> simple.log
# Show collected logs
cat stdout.log
```

2.完整模式Docker启动

```shell
make docker
docker run -d --name ilogtail-ds -v core/example_config/user_yaml_config.d:/usr/local/ilogtail/user_yaml_config.d aliyun/ilogtail:local-build
```

3.完整模式K8s启动

```shell
VERSION=snapshot make docker
kubectl apply -f xxx-configmap.yaml
kubectl apply -f xxx-deployment.yaml
```

4.纯插件模式本地启动

```shell
make pluin_main && sh output/ilogtail
```

> **注意**: 对一些高版本Linux需要提前安装systemd-devel
> ```shell
> #centos
> yum install systemd-devel
> 
> #ubuntu
> apt-get update && apt-get install -y libsystemd-dev
> ```

5.阿里云启动

请阅读此 [doc](https://help.aliyun.com/document_detail/65018.html)。

# 文档

有关最新版本的文档，请参阅 [文档索引](./docs/zh/README.md)

- [Input 插件](./docs/zh/guides/How-to-write-input-plugins.md)
- [Processor 插件](./docs/zh/guides/How-to-write-processor-plugins.md)
- [Aggregator 插件](./docs/zh/guides/How-to-write-aggregator-plugins.md)
- [Flusher 插件](./docs/zh/guides/How-to-write-flusher-plugins.md)
## iLogtail 性能测试
- [容器场景iLogtail 与Filebeat 性能对比测试](./docs/zh/performance/Performance-compare-with-filebeat.md)

## iLogtail 使用案例
- [iLogtail使用入门-主机环境日志采集到SLS](./docs/zh/usecases/How-to-setup-on-host.md)
- [iLogtail使用入门-K8S环境日志采集到SLS](./docs/zh/usecases/How-to-setup-in-k8s-environment.md)
- [iLogtail使用入门-iLogtail本地配置模式部署(For Kafka Flusher)](./docs/zh/usecases/How-to-local-deploy-kafka-flusher.md)
- [iLogtail使用入门-如何采集Prometheus Exporter数据](./docs/zh/usecases/How-to-use-prometheus-fetcher.md)
- [iLogtail使用入门-如何采集 Telegraf 数据](./docs/zh/usecases/How-to-use-telegraf-receiver.md)

# 贡献

- [修复和报告错误](https://github.com/alibaba/ilogtail/issues)
- [改进文档](https://github.com/alibaba/ilogtail/labels/documentation)
- [审查代码和功能提案](https://github.com/alibaba/ilogtail/pulls)
- [贡献插件](./docs/zh/guides/README.md)

# 联系我们
您可以通过[Github Issues](https://github.com/alibaba/ilogtail/issues) 报告bug、提出建议或参与讨论，或通过以下方式联系我们：

- 钉钉：iLogtail社区
- 微信：日志服务
- 哔哩哔哩：[阿里云SLS](https://space.bilibili.com/630680534?from=search&seid=2845737427240690794&spm_id_from=333.337.0.0)
- 知乎：[阿里云日志服务](https://www.zhihu.com/people/a-li-yun-ri-zhi-fu-wu)

<img src="https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/ilogtail-contact.png?versionId=CAEQOhiBgICQkM6b8xciIDcxZTU5M2FjMDAzODQ1Njg5NjI3ZDc4M2FhOTZkNWNk" style="width: 100%; height: 100%" />

# 我们的用户

数以万计的公司在阿里云、线下IDC、其他云等多种环境中使用 iLogtail。 更多详情请看[这里](https://help.aliyun.com/document_detail/250269.html) 。

# Licence
[Apache 2.0 许可证](./LICENSE)
