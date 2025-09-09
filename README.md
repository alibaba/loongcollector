# LoongCollector - High-Performance Observability Data Collector

<div align="center">

![LoongCollector Logo](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/logo/jpg/black-blue.jpg)

**Fast, Lightweight, and Battle-Tested Observability Agent**

[![GitHub contributors](https://img.shields.io/github/contributors/alibaba/loongcollector)](https://github.com/alibaba/loongcollector/contributors)
[![GitHub stars](https://img.shields.io/github/stars/alibaba/loongcollector)](https://github.com/alibaba/loongcollector/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/alibaba/loongcollector)](https://github.com/alibaba/loongcollector/issues)
[![GitHub license](https://img.shields.io/github/license/alibaba/loongcollector)](https://github.com/alibaba/loongcollector/blob/main/LICENSE)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/28764.svg)](https://scan.coverity.com/projects/alibaba-ilogtail)
[![Coverage Status](https://codecov.io/gh/alibaba/loongcollector/branch/main/graph/badge.svg)](https://codecov.io/gh/alibaba/loongcollector)
[![Go Report Card](https://goreportcard.com/badge/github.com/alibaba/loongcollector)](https://goreportcard.com/report/github.com/alibaba/loongcollector)

[Quick Start](#-quick-start) • [Documentation](#-documentation) • [Performance Benchmarks](#-performance-benchmarks) • [Contributing](#-contributing)

</div>

---

## Why LoongCollector?

LoongCollector is a high-performance, lightweight observability data collector designed for modern cloud-native environments. Born from Alibaba's 15-year journey handling exponential traffic growth and powering tens of millions of deployments, LoongCollector delivers **10x higher throughput** with **80% lower resource usage** compared to open-source alternatives.

LoongCollector is a key component of LoongSuite(Alibaba's unified observability data collection suite). LoongSuite includes the following key components:

* [LoongCollector](https://github.com/alibaba/loongcollector): universal node agent, which prodivdes log collection, prometheus metric collection, and network and security collection capabilities based on eBPF.
* [LoongSuite Python Agent](https://github.com/alibaba/loongsuite-python-agent): a process agent providing instrumentaion for python applications.
* [LoongSuite Go Agent](https://github.com/alibaba/loongsuite-go-agent): a process agent for golang with compile time instrumentation.
* [LoongSuite Java Agent](https://github.com/alibaba/loongsuite-java-agent): a process agent for Java applications.
* Other upcoming language agent.

### 🚀 Core Advantages

- **⚡ High Performance**
  - 10x higher throughput with 80% less resource usage compared to competitors
  - Linear scaling with predictable performance growth

- **🛡️ Production-Ready**
  - Battle-tested in Alibaba's production environment for 15+ years
  - Powers tens of millions of deployments, collecting hundreds of petabytes daily

- **🔧 All-in-One Collection**
  - Unified agent for logs, metrics, traces, events, and profiles
  - Native Kubernetes support
  - eBPF-powered network monitoring and security event collection

- **🎯 Pluggable Architecture**
  - 100+ built-in plugins with multi-language development (C++, Go)
  - Powerful SPL engine for flexible data processing

- **⚙️ Advanced Management**
  - Remote configuration management via SLS console, SDK, K8s Operator
  - Self-monitoring, flow control, resource control, alarms, and statistics collection.

### 📊 Performance Benchmarks

**Maximum Throughput Comparison**

| Log Type | LoongCollector | FluentBit | Vector | Filebeat |
|----------|----------------|-----------|--------|----------|
| Single Line | **546 MB/s** | 36 MB/s | 38 MB/s | 9 MB/s |
| Multi-line | **238 MB/s** | 24 MB/s | 22 MB/s | 6 MB/s |
| Regex Parsing | **68 MB/s** | 19 MB/s | 12 MB/s | Not Supported |

*📈 **Breaking Point Analysis**: While competitors hit CPU saturation at ~40 MB/s, LoongCollector maintains linear scaling up to **546 MB/s** on a single processing thread.*


**Resource Efficiency at 10 MB/s Processing Load**

| Scenario | LoongCollector | FluentBit | Vector | Filebeat |
|----------|----------------|-----------|--------|----------|
| Simple Line (512B) | 3.40% CPU<br>29.01 MB RAM | 12.29% CPU (+261%)<br>46.84 MB RAM (+61%) | 35.80% CPU (+952%)<br>83.24 MB RAM (+186%) | **Performance Insufficient** |
| Multi-line (512B) | 5.82% CPU<br>29.39 MB RAM | 28.35% CPU (+387%)<br>46.39 MB RAM (+57%) | 55.99% CPU (+862%)<br>85.17 MB RAM (+189%) | **Performance Insufficient** |
| Regex (512B) | 14.20% CPU<br>34.02 MB RAM | 37.32% CPU (+162%)<br>46.44 MB RAM (+36%) | 43.90% CPU (+209%)<br>90.51 MB RAM (+166%) | **Not Supported** |

## 🏗️ Performance And Reliability Architecture Highlights

**1. Memory Arena: Zero-Copy Design**
- Shared memory pool (SourceBuffer) stores all string data once per event group
- String_view references point to original data segments instead of copying

**2. Lock-Free Event Pool**
- Thread-aware allocation strategies eliminate lock contention
- Same-thread pools for direct reuse, double-buffer pools for cross-thread scenarios

**3. Zero-Copy Serialization: Direct Network Output**
- Bypasses intermediate Protobuf objects, serializes directly to network format

**4. Multi-Tenant Pipeline Isolation**
- High-low watermark feedback queues prevent pipeline interference
- Independent resource allocation per pipeline with automatic back-pressure control
- Ensures one pipeline failure doesn't affect others

**5. Fair Resource Allocation**
- Priority-aware round-robin scheduling ensures fairness while respecting business priorities
- Higher priority pipelines always processed first, fair distribution within same priority level
- Automatic resource yielding when constraints occur

**6. Self-Healing Network Resilience**
- Adaptive concurrency limiting per destination using AIMD (Additive Increase, Multiplicative Decrease)
- Fast failure detection and gradual recovery to prevent network jitter
- Zero data loss guarantee with intelligent back-pressure control

## 🏭 Production Validation: Battle-Tested at Scale

LoongCollector has been battle-tested in some of the world's most demanding production environments:

- **Alibaba Group**: Powers the entire Alibaba ecosystem including Double 11 shopping festival
- **Alibaba Cloud**: Serves tens of thousands of enterprise customers
- **Ant Group**: Handles financial transaction observability at massive scale
- **Daily Data Volume**: Hundreds of petabytes of observability data
- **Deployment Scale**: Tens of millions of active deployments

## 🚀 Quick Start

### Prerequisites
- Docker (for building from source)
- Go 1.19+ (for building from source)

### Build and Run

```bash
# Clone the repository
git clone https://github.com/alibaba/loongcollector.git
cd loongcollector
git submodule update --init

# Build LoongCollector
make all
cd output

# Start LoongCollector
nohup ./loongcollector > stdout.log 2> stderr.log &
```

LoongCollector is now running.

### Docker Quick Start

```bash
# Build the Docker image alibaba/loongcollector:0.0.1
make dist
make docker

# Run with default configuration
docker run -d --name loongcollector \
  -v /:/logtail_host:ro \
  -v /var/run:/var/run \
  alibaba/loongcollector:0.0.1
```

## 📚 Documentation

- **[User Manual](https://observability.cn/project/loongcollector/readme/)** - Comprehensive documentation
- **[Installation Guide](https://observability.cn/project/loongcollector/quick-start/)** - Step-by-step setup
- **[Configuration Reference](https://observability.cn/project/loongcollector/collection-config/)** - Detailed configuration options
- **[Plugin Directory](https://observability.cn/project/loongcollector/overview/)** - Complete plugin documentation
- **[Developer Guide](https://observability.cn/project/loongcollector/development-environment/)** - Contributing and development

## 🤝 Contributing

We welcome contributions from the community! Here are some ways you can help:

- **[Report Bugs](https://github.com/alibaba/loongcollector/issues)** - Help us identify and fix issues
- **[Improve Documentation](https://github.com/alibaba/loongcollector/labels/documentation)** - Enhance our docs
- **[Review Code](https://github.com/alibaba/loongcollector/pulls)** - Review feature proposals and PRs
- **[Contribute Plugins](https://github.com/alibaba/loongcollector/issues)** - Develop new input, processor, or flusher plugins

## 📞 Contact Us

- **GitHub Issues**: [Report bugs and feature requests](https://github.com/alibaba/loongcollector/issues)
- **GitHub Discussions**: [Join community discussions](https://github.com/alibaba/loongcollector/discussions)

### Community Channels

- **Bilibili**: [阿里云SLS](https://space.bilibili.com/630680534)
- **Zhihu**: [LoongCollector社区](https://www.zhihu.com/column/c_1533139823409270785)
- **WeChat/DingTalk**: LoongCollector社区

<div align="center">
<img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/chatgroup/chatgroup.png" alt="Community QR Code" width="60%"/>
</div>

### Other LoongSuite components's Community contact information

We are looking forward to your feedback and suggestions. You can scan the QR code below to engage with us.

| LoongSuite Python SIG | LoongSuite Go SIG | LoongSuite Java SIG |
|----|----|----|
| <img src="https://github.com/alibaba/loongsuite-python-agent/blob/main/docs/_assets/img/loongsuite-python-sig-dingtalk.jpg" height="150"> | <img src="https://github.com/alibaba/loongsuite-python-agent/blob/main/docs/_assets/img/loongsuite-go-sig-dingtalk.png" height="150"> | <img src="https://github.com/alibaba/loongsuite-python-agent/blob/main/docs/_assets/img/loongsuite-java-sig-dingtalk.jpg" height="150"> |

## 📄 License

LoongCollector is licensed under the [Apache 2.0 License](./LICENSE).

---

<div align="center">

**Built with ❤️ by the Alibaba Cloud Observability Team**

*Empowering developers to build better observability solutions*

</div>
