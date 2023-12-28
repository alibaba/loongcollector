# 使用介绍

## 简介

目前可观测采集 Agent，例如 iLogtail，主要提供了本地采集配置管理模式，当涉及实例数较多时，需要逐个实例进行配置变更，管理比较复杂。此外，多实例场景下 Agent 的版本信息、运行状态等也缺乏统一的监控。因此，需要提供全局管控服务用于对可观测采集 Agent 的采集配置、版本信息、运行状态等进行统一的管理。

ConfigServer 就是这样的一款可观测 Agent 管控工具，目前支持：

* 采集 Agent 注册到 ConfigServer
* 以 Agent 组的形式对采集 Agent 进行统一管理
* 远程批量配置采集 Agent 的采集配置
* 监控采集 Agent 的运行状态

## 术语表

* 采集 Agent：数据采集器。可以是 iLogtail，或者其他的采集器。
* 采集配置：一个数据采集 Pipeline，对应一组独立的数据采集配置。
* AgentGroup：可以将相同属性的Agent划分为一个组，只需要绑定采集配置到 AgentGroup 即可在组内所有 Agent 生效。目前仅支持单 AgentGroup（即默认的 AgentGroup `default`）。
* ConfigServer：采集配置管控的服务端。

## 功能描述

对采集 Agent 进行全局管控。任何服从协议的Agent，都可以受到统一的管控。

### 实例注册

* Agent 侧配置已部署的 ConfigServer 信息。
* Agent 启动后，定期向 ConfigServer 进行心跳注册，证明存活性。
* 上报包括但不限于如下的信息，供 ConfigServer 汇集后统一通过API对外呈现。
  * Agent 的 instance_id，作为唯一标识。
  * 版本号
  * 启动时间
  * 运行状态

### AgentGroup 管理

* Agent 管理的基本单元是 AgentGroup ，采集配置通过关联 AgentGroup 生效到组内的 Agent 实例上。
* 系统默认创建 default 组，所有 Agent 也会默认加到 default 组。
* 支持通过 AgentGroup 的 Tag 实现自定义分组。如果 Agent 的 Tag 与 AgentGroup 的 Tag 匹配，则加入该组。
* 一个 AgentGroup 可以包含多个 Agent，一个 Agent 可以属于多个组。

### 采集配置全局管控

* 采集配置通过API进行服务端配置，之后与 AgentGroup 绑定后，即可生效到组内 Agent 实例上。
* 通过采集配置版本号区分采集配置差异，增量变更到 Agent 侧。

### 状态监控

* Agent 定期向 ConfigServer 发送心跳，上报运行信息。
* ConfigServer 汇集后统一通过API对外呈现。

## 运行

ConfigServer 分为 UI 和 Service 两部分，可以分别独立运行。

### Agent 配置

Agent 侧需要配置 ConfigServer 信息，才能使用管控功能。

#### iLogtail 配置 ConfigServer

打开 iLogtail 目录下的 ilogtail_config.json 文件，配置 ConfigServer 相关参数 `ilogtail_configserver_address` 和 `ilogtail_tags`。

`ilogtail_configserver_address` 是 ConfigServer 部署的地址与端口号，可以配置多个 ConfigServer 地址， iLogtail 将自动切换选择可以链接的 ConfigServer。需要注意的是，目前的 ConfigServer 仅支持单机版，`ilogtail_configserver_address` 即使配置了多个地址，多个 ConfigServer 之间也并不支持数据同步。我们预留了 ConfigServer 支持分布式部署的扩展性，欢迎社区积极贡献开发。

`ilogtail_tags` 是 iLogtail 在 ConfigServer 处的标签，支持配置多个。虽然该参数暂时无法使用，但我们同样预留了支持通过自定义标签分组管理 Agent 的扩展性。

下面是一个简单的配置示例。

```json
{
    ...
    "ilogtail_configserver_address" : [
      "127.0.0.1:8899"
      ],
    ...
}
```

### Service

Service 为分布式的结构，支持多地部署，负责与采集 Agent 和用户/ui 通信，实现了管控的核心能力。

#### 启动

从 GitHub 下载 iLogtail 源码，进入 ConfigServer 后端目录下，编译运行代码。

``` bash
cd config_server/service
go build -o ConfigServer
nohup ./ConfigServer > stdout.log 2> stderr.log &
```

#### 配置选项

配置文件为 `config_server/service/seeting/setting.json`。可配置项如下：

* `ip`：Service 服务启动的 ip 地址，默认为 `127.0.0.1`。
* `port`：Service 服务启动的端口，默认为 `8899`。
* `store_mode`：数据持久化的工具，默认为 `leveldb`。当前仅支持基于 leveldb 的单机数据持久化。
* `db_path`：数据持久化的存储地址或数据库连接字符串，默认为 `./DB`。
* `agent_update_interval`：将采集 Agent 上报的数据批量写入存储的时间间隔，单位为秒，默认为 `1` 秒。
* `config_sync_interval`：Service 从存储同步采集 Config 的时间间隔，单位为秒，默认为 `3` 秒。

配置样例：

```json
{
    "ip":"127.0.0.1",
    "store_mode": "leveldb",
    "port": "8899",
    "db_path": "./DB",
    "agent_update_interval": 1,
    "config_sync_interval": 3
}
```

### UI

UI 为一个 Web 可视化界面，与 Service 连接，方便用户对采集 Agent 进行管理。

#### 快速开始

```shell
git clone https://github.com/iLogtail/config-server-ui
cd config-server-ui
yarn install
yarn start
```

#### 更多信息

请参考[这里](https://github.com/iLogtail/config-server-ui/blob/master/README.md)
