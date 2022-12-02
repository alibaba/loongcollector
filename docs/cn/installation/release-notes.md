# 发布记录

## 1.3.0

### 发布记录

发版日期：2022 年 11 月 30 日

新功能

* 新增Kafka新版本输出插件，支持自定义输出格式 [#218](https://github.com/alibaba/ilogtail/issues/218)
* 支持通过grpc输出Open Telemetry协议日志 [#418](https://github.com/alibaba/ilogtail/pull/418)
* 支持通过HTTP接入Open Telemetry协议日志 [#421](https://github.com/alibaba/ilogtail/issues/421)
* 新增脱敏插件processor\_desensitize [#525](https://github.com/alibaba/ilogtail/pull/525)
* 支持写入SLS时使用zstd压缩编码 [#526](https://github.com/alibaba/ilogtail/issues/526)

优化

* 减小发布二进制包大小 [#433](https://github.com/alibaba/ilogtail/pull/433)
* 使用ENV方式创建SLS资源时支持使用HTTPS协议 [#505](https://github.com/alibaba/ilogtail/issues/505)
* 支持创建SLS Logstore时选择query mode [#502](https://github.com/alibaba/ilogtail/issues/502)
* 使用插件处理时也支持输出内容在文件内偏移量 [#395](https://github.com/alibaba/ilogtail/issues/395)
* 默认支持采集容器标准输出时上下文保持连续 [#522](https://github.com/alibaba/ilogtail/pull/522)
* Prometheus数据接入内存优化 [#524](https://github.com/alibaba/ilogtail/pull/524)

问题修复

* 修复Docker环境下潜在的FD泄露和事件遗漏问题 [#529](https://github.com/alibaba/ilogtail/issues/529)
* 修复配置更新时文件句柄泄露的问题 [#420](https://github.com/alibaba/ilogtail/issues/420)
* IP在特殊主机名下解析错误 [#517](https://github.com/alibaba/ilogtail/pull/517)
* 修复多个配置路径存在父子目录关系时文件重复采集的问题 [#533](https://github.com/alibaba/ilogtail/issues/533)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.3.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.3.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.3.0/ilogtail-1.3.0.linux-amd64.tar.gz) | Linux | x86-64 | - |
| [ilogtail-1.3.0.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.3.0/ilogtail-1.3.0.linux-arm64.tar.gz) | Linux | arm64  | - |

### Docker 镜像

**Docker Pull 命令**&#x20;

```
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.1.0-k8s-patch
```

## 1.2.1

### 发布记录

发版日期：2022 年 9 月 28 日

新功能

* 新增PostgreSQL采集插件。
* 新增SqlServer采集插件。
* 新增Grok处理插件。
* 支持JMX性能指标采集。
* 新增日志上下文聚合插件，可支持插件处理后的上下文浏览和日志topic提取。

优化

* 支持采集秒退容器的标准输出采集。
* 缩短已退出容器句柄释放时间。

问题修复
* 飞天日志格式微妙时间戳解析。

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.2.1.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.2.1.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.2.1/ilogtail-1.2.1.linux-amd64.tar.gz) | Linux | x86-64 | 8a5925f1bc265fd5f55614fcb7cebd571507c2c640814c308f62c498b021fe8f |
| [ilogtail-1.2.1.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.2.1/ilogtail-1.2.1.linux-arm64.tar.gz) | Linux | arm64  | 04be03f03eb722a3c9ba1b45b32c9be8c92cecabbe32bdc4672d229189e80c2f |

### Docker 镜像

**Docker Pull 命令**&#x20;

```
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.1.0-k8s-patch
```

## 1.1.1

### 发布记录

发版日期：2022 年 8 月 22 日

新功能

* 新增Logtail CSV处理插件。
* 支持通过eBPF进行四层、七层网络流量分析，支持HTTP、MySQL、PgSQL、Redis、DNS协议。

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.1.1.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.1.1.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.1.1/ilogtail-1.1.1.linux-amd64.tar.gz) | Linux | x86-64 | 2330692006d151f4b83e4b8be0bfa6b68dc8d9a574c276c1beb6637c4a2939ec |

### Docker 镜像

**Docker Pull 命令**&#x20;

```
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.1.0-k8s-patch
```

## 1.1.0

### 发布记录

发版日期：2022 年 6 月 29 日

新功能

* 开源C++核心模块代码
* netping插件支持httping和DNS解析耗时。

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.1.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.1.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.1.0/ilogtail-1.1.0.linux-amd64.tar.gz) | Linux | x86-64 | 2f4eadd92debe17aded998d09b6631db595f5f5aec9c8ed6001270b1932cad7d |

### Docker 镜像

**Docker Pull 命令**&#x20;

{% hint style="warning" %}
tag为1.1.0的镜像缺少必要环境变量，无法支持容器内文件采集和checkpoint，请使用1.1.0-k8s-patch或重新拉取latest
{% endhint %}

```
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.1.0-k8s-patch
```
