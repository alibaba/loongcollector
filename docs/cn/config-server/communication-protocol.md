# 通信协议

服从HTTP/HTTPS协议。

## 通用参数

ip 和 port 为 ConfigServer 的 ip 和接收端口，默认为127.0.0.1和8899。

## API 响应规范

数据的发送及接收均为 protocol buffer 格式，参见源代码中的 [proto](github.com/alibaba/ilogtail/config_server/service/proto) 文件夹（阿尔法版，后续可能发生不兼容变更）。

在调用API接口过程中，若返回HTTP状态码为 200，则表示请求成功，若返回HTTP状态码为其他，例如404等，则表示调用API接口失败。服务端会返回请求响应信息如下。code为响应码，表示请求成功/失败的类型；message 是响应信息，包含详细的请求成功信息/失败说明。

```json
{
  "code" : <Code>,
  "message" : <Message>
}
```

主要响应码如下所示：

| HTTP状态码 | 响应码 | 说明 |
|-|-|-|
|200 | Accept | 请求成功 |
|400 | AgentGroupAlreadyExist | Agent组已存在 |
|400 | ConfigAlreadyExist | 采集配置已存在 |
|400 | AgentAlreadyExist | Agent已存在 |
|400 | InvalidParameter | 无效的参数 |
|400 | BadRequest | 参数不符合要求 |
|404 | AgentGroupNotExist | Agent组不存在 |
|404 | ConfigNotExist | 采集配置不存在 |
|404 | AgentNotExist | Agent不存在 |
|500 | InternalServerError | 内部服务调用错误 |
|500 | RequestTimeout | 请求超时 |
|503 | ServerBusy | 服务器正忙 |

## 通信时常见数据类型说明

### Agent

通信传输中的采集 Agent 信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| agent_id | string | Agent 的唯一标识 |
| agent_type | string | Agent 的类型 |
| ip | string | Agent 的 ip 地址 |
| version | string | Agent 的版本 |
| running_status | string | Agent 的运行状态 |
| startup_time | int64 | Agent 的启动时间 |
| latest_heartbeat_time | int64 | Agent 的上次心跳时间 |
| tags | map<string, string>  | Agent 的标签 |
| running_details | map<string, string>  | Agent 的运行详细信息 |

### AgentGroup

通信传输中的 AgentGroup 信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| group_name | string | AgentGroup 的唯一标识 |
| description | string | AgentGroup 的备注说明 |
| tags | AgentGroupTag[] | AgentGroup 的标签数组 |

### AgentGroupTag

通信传输中的 AgentGroup 的 Tag 信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| name | string | AgentGroupTag 的标签名 |
| value | string | AgentGroupTag 的标签值 |

### Config

通信传输中的采集 Config 的信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| config_name | string | Config 的唯一标识 |
| agent_type | string | Config 适配的 Agent 类型 |
| description | string | Config 的备注说明 |
| content | string | Config 的详细数据 |

### ConfigUpdateInfo

通信传输中采集 Agent 拉取的采集 Config 的更新信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| config_name | string | Config 的唯一标识 |
| update_status | enum | Config 的更新状态，共有4种：<br>SAME：一致<br>NEW：新增<br>DELETED：删除<br>MODIFIED：修改 |
| config_version | int64 | Config 的版本号 |
| content | string | Config 的详细数据 |

### RunningStatistics

通信传输中采集 Agent 上报的运行状态信息

| 参数 | 类型 | 说明 |
| - | - | - |
| cpu | float | Agent 的 cpu 状态 |
| memory | int64 | Agent 的内存使用状态 |
| extras | map<string, string> | Agent 的其他运行状态信息 |

## 接口说明

以API的形式对外提供统一的管控。总体分为两大类：

* 用户API
  * AgentGroup 创建/修改/删除/查看
  * 采集配置 创建/修改/删除/查看
  * 采集配置与 AgentGroup 绑定/解绑
* Agent API
  * Agent心跳/运行统计/Alarm
  * 采集配置获取

### 用户API: AgentGroup 创建/修改/删除/查看

#### `ip:port/User/CreateAgentGroup`

* 功能：新建 AgentGroup
* 请求方式：POST
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_group | AgentGoup，无默认值（必填） | 新建的 AgentGroup 的信息 |

* 返回值： 无

#### `ip:port/User/UpdateAgentGroup`

* 功能：修改已有的 AgentGroup 的信息
* 请求方式：PUT
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_group | AgentGoup，无默认值（必填） | 修改后的 AgentGroup 的信息 |

* 返回值： 无

#### `ip:port/User/DeleteAgentGroup/`

* 功能：删除已建立的 AgentGroup
* 请求方式：DELETE
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| group_name | string，无默认值（必填） | 要删除的 AgentGroup 的名称 |

* 返回值： 无

#### `ip:port/User/GetAgentGroup/`

* 功能：查看 AgentGroup 的信息
* 请求方式：GET
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| group_name | string，无默认值（必填） | 要获取的 AgentGroup 的名称 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| agent_group | AgentGroup | 目标 AgentGroup 的信息 |

#### `ip:port/User/ListAgentGroups/`

* 功能：获取所有的 AgentGroup 列表
* 请求方式：GET
* 参数：无
* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| agent_groups | AgentGroup[] | 包含所有 AgentGroup 信息的数组 |

### 用户API：采集配置 创建/修改/删除/查看

#### `ip:port/User/CreateConfig/`

* 功能：导入本地 Config
* 请求方式：POST
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| config_detail | Config，无默认值（必填） | 新增的 Config 的信息 |

* 返回值： 无

#### `ip:port/User/UpdateConfig/`

* 功能：修改已有的 Config 的信息
* 请求方式：PUT
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| config_detail | Config，无默认值（必填） | 修改后的 Config 的信息 |

* 返回值： 无

#### `ip:port/User/DeleteConfig/`

* 功能：删除已保存的 Config
* 请求方式：DELETE
* 参数：  

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| config_name | string，无默认值（必填） | 要删除的 Config 的名称 |

* 返回值： 无

#### `ip:port/User/GetConfig/`

* 功能：查看指定的 Config 的信息
* 请求方式：GET
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| config_name | string，无默认值（必填） | 要获取的 config 名称 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| config_detail | Config | 目标 Config 的信息 |

#### `ip:port/User/ListConfigs/`

* 功能：获取所有的 Config 列表
* 请求方式：GET
* 参数：无
* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| config_details | Config[] | 包含所有 Config 信息的数组 |

### 用户API：采集配置与 AgentGroup  绑定/解绑  

#### `ip:port/User/ApplyConfigToAgentGroup/`

* 功能：向 AgentGroup 中添加 Config
* 请求方式：PUT
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| group_name | string，无默认值（必填） | 目标 AgentGroup 的名字 |
| config_name | string，无默认值（必填） | 目标 Config 的名字 |

* 返回值： 无

#### `ip:port/User/RemoveConfigFromAgentGroup/`

* 功能：删除 AgentGroup 中的 Config
* 请求方式：DELETE
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| group_name | string，无默认值（必填） | 目标 AgentGroup 的名字 |
| config_name | string，无默认值（必填） | 目标 Config 的名字 |

* 返回值： 无

#### `ip:port/User/GetAppliedConfigsForAgentGroup/`

* 功能：查看 AgentGroup 中的 Config 列表
* 请求方式：GET
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| group_name | string，无默认值（必填） | 目标 AgentGroup 名称 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| config_names | string[] | AgentGroup 关联的所有 Config 名称的数组 |

#### `ip:port/User/GetAppliedAgentGroups/`

* 功能：查看 Config 关联的 AgentGroup 列表
* 请求方式：GET
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| config_name | string，无默认值（必填） | 目标 Config 名称 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| agent_group_names | string[] | Config 关联的所有 AgentGroup 名称的数组 |

#### `ip:port/User/ListAgents/`

* 功能：获取 AgentGroup 中 Agent 列表
* 请求方式：GET
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| group_name | string，无默认值（必填） | 目标 AgentGroup 名称 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| agents | Agent[] | 包含 AgentGroup 中所有的 Agent 信息及其状态的数组 |

### Agent API：写入心跳、状态统计、Alarm

#### `ip:port/Agent/HeartBeat/`

* 功能：Agent 向 ConfigServer发送心跳
* 请求方式：POST
* 参数：  

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_id | string，无默认值（必填） | Agent 的唯一标识符 |
| agent_type | string，无默认值（必填） | Agent 的种类 |
| agent_version | string，无默认值（必填） | Agent 的版本 |
| ip | string，无默认值（必填） | Agent 的 ip 地址 |
| tags | map<string, string>，无默认值（必填） | Agent 拥有的 tag |
| running_status | string，无默认值（必填） | Agent 的运行状态 |
| startup_time | int64，无默认值（必填） | Agent 的启动时间 |

* 返回值： 无

#### `ip:port/Agent/RunningStatistics/`

* 功能：iLogtail 向 ConfigServer  发送自身运行状态和 Config 读取进度（iLogtail 专属）
* 请求方式：POST
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_id | string，无默认值（必填） | Agent 的唯一标识符 |
| running_details | RunningStatistics，无默认值（必填） | Agent 的运行详细信息 |

* 返回值： 无

#### `ip:port/Agent/Alarm/`

* 功能：Agent 向 ConfigServer  发送运行告警
* 请求方式：POST
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_id | string，无默认值（必填） | Agent 的唯一标识符 |
| type | string，无默认值（必填） | 警报类型 |
| detail | string，无默认值（必填） | 警报内容 |

* 返回值： 无

### Agent API：根据 Agent 获取采集配置（含变更信息）

#### `ip:port/Agent/GetConfigList/`

* 功能：Agent 向 ConfigServer 发送已有的 Config 列表，获取更新
* 请求方式：POST
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_id | string，无默认值（必填） | Agent 的唯一标识符 |
| config_versions | map<string, string>，无默认值（必填） | Agent 当前的配置文件信息，key为 Config 的名称，value 为版本号 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| config_update_infos | ConfigUpdateInfo[] | Agent 拉取的 Config 更新信息 |
