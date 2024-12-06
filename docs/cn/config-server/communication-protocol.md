# 通信协议

由iLogtail社区联合哔哩哔哩共同建设。服从HTTP/HTTPS协议。

## 通用参数

ip 和 port 为 ConfigServer 的 ip 和接收端口，默认为127.0.0.1和8899。

## API 响应规范

数据的发送及接收均为 protocol buffer 格式，参见源代码中的 [config_server/protocol](https://github.com/alibaba/loongcollector/tree/main/config_server/protocol) 文件夹（v1.0）。

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
|400 | INVALID_PARAMETER | 无效的参数 |
|500 | INTERNAL_SERVER_ERROR | 内部服务调用错误 |

## 通信时常见数据类型说明

### Agent

通信传输中的采集 Agent 信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| agent_type | string | Agent 的类型 |
| agent_id | string | Agent 的唯一标识 |
| attributes | AgentAttributes | Agent 的运行信息 |
| string | string[] | Agent 的标签 |
| running_status | string | Agent 的运行状态 |
| startup_time | int64 | Agent 的启动时间 |
| interval | int32 | Agent 的心跳间隔 |

### AgentAttributes

| 参数 | 类型 | 说明 |
| - | - | - |
| version | string | Agent 的版本 |
| category | string | Agent 的类型（用于拉取AGENT_CONFIG） |
| ip | string | Agent 的IP |
| hostname | string | Agent 的主机名 |
| region | string | Agent 的地域 |
| zone | string | Agent 的可用区 |
| extras | map<string,string> | Agent 的其他信息 |

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

### CheckStatus（枚举类型）

拉取配置文件更新时返回的更新状态。

| 值 | 说明 |
| - | - |
| NEW | 新增的配置文件 |
| DELETED | 删除的配置文件 |
| MODIFIED | 有改动的配置文件 |

### Command

Agent 从 ConfigServer 接收到的指令。

| 参数 | 类型 | 说明 |
| - | - | - |
| type | string | 指令的类型 |
| name | string | 指令名 |
| id | string | 指令的编号 |
| args | map<string,string>  | 指令的参数 |

### ConfigCheckResult

ConfigServer 返回给 Agent 的配置文件更新信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| type | ConfigType | Config 的类型 |
| name | string | Config 的唯一标识 |
| old_version | int64 | Config 的本地版本 |
| new_version | int64 | Config 的最新版本 |
| context | string | Config 的上下文 |
| check_status | CheckStatus | Config 的更新状态 |

### ConfigDetail

通信传输中的采集 Config 的信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| type | ConfigType | Config 的类型 |
| name | string | Config 的唯一标识 |
| version | int64 | Config 的版本 |
| context | string | Config 的上下文 |
| detail | string | Config 的详细数据 |

### ConfigInfo

Agent 向 ConfigServer 请求时携带的配置文件信息。

| 参数 | 类型 | 说明 |
| - | - | - |
| type | ConfigType | Config 的类型 |
| name | string | Config 的唯一标识 |
| version | int64 | Config 的版本 |
| context | string | Config 的上下文 |

### ConfigType（枚举类型）

配置文件的类型。

| 值 | 说明 |
| - | - |
| PIPELINE_CONFIG | 采集 pipeline 配置 |
| AGENT_CONFIG | Agent 运行配置 |

### RespCode（枚举类型）

通信时返回的状态码。

| 值 | 说明 |
| - | - |
| ACCEPT | 请求成功 |
| INVALID_PARAMETER | 请求失败，参数有误 |
| INTERNAL_SERVER_ERROR | 请求失败，服务内部错误 |

## 接口说明

以API的形式对外提供统一的管控。总体分为两大类：

* 用户API
  * AgentGroup 创建/修改/删除/查看
  * 采集配置 创建/修改/删除/查看
  * 采集配置与 AgentGroup 绑定/解绑
* Agent API
  * Agent心跳
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
| config_detail | ConfigDetail，无默认值（必填） | 新增的 Config 的信息 |

* 返回值： 无

#### `ip:port/User/UpdateConfig/`

* 功能：修改已有的 Config 的信息
* 请求方式：PUT
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| config_detail | ConfigDetail，无默认值（必填） | 修改后的 Config 的信息 |

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
| config_detail | ConfigDetail | 目标 Config 的信息 |

#### `ip:port/User/ListConfigs/`

* 功能：获取所有的 Config 列表
* 请求方式：GET
* 参数：无
* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| config_details | ConfigDetail[] | 包含所有 Config 信息的数组 |

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

### Agent API：写入心跳

#### `ip:port/Agent/HeartBeat/`

* 功能：Agent 向 ConfigServer发送心跳
* 请求方式：POST
* 参数：  

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_id | string，无默认值（必填） | Agent 的唯一标识符 |
| agent_type | string，无默认值（必填） | Agent 的种类 |
| attributes | AgentAttributes，无默认值（必填） | Agent 的信息 |
| tags | string[]，无默认值（必填） | Agent 拥有的 tag |
| running_status | string，无默认值（必填） | Agent 的运行状态 |
| startup_time | int64，无默认值（必填） | Agent 的启动时间 |
| interval | int32，无默认值（必填） | Agent 的心跳间隔 |
| pipeline_configs | ConfigInfo，无默认值（必填） | Agent 拥有的采集配置信息 |
| agent_configs | ConfigInfo，无默认值（必填） | Agent 拥有的运行配置信息 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| pipeline_check_results | ConfigCheckResult[] | Agent 拉取的采集配置更新信息 |
| agent_check_results | ConfigCheckResult[] | Agent 拉取的运行配置更新信息 |
| custom_commands | Command[] | Agent 接收到的命令 |

### Agent API：根据 Agent 获取采集配置（含变更信息）

#### `ip:port/Agent/FetchPipelineConfig/`

* 功能：Agent 向 ConfigServer 发送已有的采集配置列表，获取更新
* 请求方式：POST
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_id | string，无默认值（必填） | Agent 的唯一标识符 |
| req_configs | ConfigInfo[] | Agent 需要拉取的配置文件信息 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| config_details | ConfigDetail[] | Agent 拉取的配置文件完整信息 |

#### `ip:port/Agent/FetchAgentConfig/`

* 功能：Agent 向 ConfigServer 发送已有的运行配置列表，获取更新
* 请求方式：POST
* 参数：

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| agent_id | string，无默认值（必填） | Agent 的唯一标识符 |
| attributes | AgentAttributes，无默认值（必填） | Agent 的信息 |
| req_configs | ConfigInfo[] | Agent 需要拉取的配置文件信息 |

* 返回值：

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| config_details | ConfigDetail[] | Agent 拉取的配置文件完整信息 |
