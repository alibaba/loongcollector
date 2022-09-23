# 发布记录

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
## 1.2.0

### 发布记录

发版日期：2022 年 9 月 23 日

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

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.2.0.md)
