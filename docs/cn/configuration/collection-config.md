# 采集配置（collection pipeline）

在 LoongCollector 中，**采集配置**（英文 **collection pipeline**；代码与实现中也常简称 **pipeline**）指一组 `inputs` / `processors` / `aggregators` / `flushers`（及可选 `extensions`）组成的采集与投递定义，通常对应**一个配置文件**。

## 存放位置与类别

| 类别 | 说明 | 默认目录（相对 `conf/`，本地示意） |
|------|------|--------------------------------------|
| **持续采集配置** | 长期运行、可持续热加载更新 | `./continuous_pipeline_config/local`（及按需的 `remote/`） |
| **一次性采集配置** | 运行完毕即结束的离线/单次任务 | `./onetime_pipeline_config/local` |

上述路径位于 LoongCollector **配置根目录**（常为 `$LOONGCOLLECTOR_ROOT/conf` 或安装包下的 `conf/`）。开启 `logtail_mode` 等兼容布局时，持续采集配置的目录名在实现上可能映射为 `config/`，以运行环境为准。

**实例级配置（instance_config）** 与采集配置是不同概念：用于进程级参数（如资源阈值、默认凭证、扫描间隔等），位于 **`conf/instance_config/`**（如 `local/loongcollector_config.json`），**不**放在 `continuous_pipeline_config` 或 `onetime_pipeline_config` 下。详见 [系统参数](system-config.md) 与 [目录结构说明](../installation/loongcollector-dir.md)。

## 格式

采集配置定义文件支持 **JSON** 与 **YAML**（扩展名与解析语义以实际部署版本为准）。**本文档、`example_config/data_pipelines` 与 E2E 中的示例默认采用 YAML**；历史 JSON 样例维护时应逐步改为 YAML。

单篇插件文档中的约定：**采集配置**相关代码块使用 YAML；**「输出」**（序列化后的一条或多条数据示意）使用 JSON，与常见行协议一致。

每个采集配置的一级字段如下（与实现不一致时以当前版本源码为准：`core/config/CollectionConfig.cpp`、`core/collection_pipeline/GlobalConfig.*`、`pkg/config/global_config.go`）。

| **参数**                           | **类型**     | **是否必填** | **默认值** | **说明**                          |
|----------------------------------|------------|----------|---------|---------------------------------|
| enable                           | bool       | 否        | true    | 是否启用当前采集配置。                       |
| global                           | object     | 否        | 空       | 采集配置级全局参数，C++ 与 Go 侧都会读取；未列出的键可能作为扩展字段传给 Go 插件（见 `GlobalConfig::Init` 与 `extendedParams`）。                           |
| global.StructureType             | string     | 否        | v1      | 插件系统配置结构版本：`v1` 或 `v2`（影响 Go 侧 `PluginRunner` 类型；未设置时按 v1）。               |
| global.InputIntervalMs           | int        | 否        | 1000    | **扩展（Go）Input** 侧默认调度间隔，单位毫秒（不限于 MetricInput；与 `pkg/config.GlobalConfig.InputIntervalMs` 默认一致）。               |
| global.InputMaxFirstCollectDelayMs| int       | 否        | 10000   | 扩展 Input 首次采集前随机等待上限（毫秒）；若大于 `InputIntervalMs` 则实际按间隔封顶（见 Go `GlobalConfig` 注释）。               |
| global.EnableTimestampNanosecond | bool       | 否        | false   | 是否在序列化等路径中启用纳秒时间戳（C++ `GlobalConfig` 与传给 Go 的全局字段一致）。               |
| global.UsingOldContentTag        | bool       | 否        | false   | 是否使用旧版 `content` Tag 行为（C++ `GlobalConfig`）。 |
| global.PipelineMetaTagKey        | object     | 否        | 空       | 重命名或删除采集配置级 Tag。map 中 key 为原 tag 逻辑名（如 `HOST_NAME`），value 为新物理 tag 名；空串表示删除，`__default__` 表示默认值。可配置项见下文表 1。 |
| global.AgentEnvMetaTagKey        | object     | 否        | 空       | 环境变量元数据 Tag 映射（`ProcessorTagNative` 等读取此键名）。C++ `GlobalConfig::sNativeParam` 中还包含 **`AgentMetaTagKey`**，用于与 Go 管线 JSON 交互；若与文档/示例不一致，以当前版本源码及单测为准。 |
| global.ExcutionTimeout           | uint       | 条件      | —       | **仅一次性采集配置**：运行超时（秒），键名为源码中的 `ExcutionTimeout`（拼写与实现对齐）；范围在代码中裁剪为约 600～604800。 |
| global.ForceRerunWhenUpdate      | bool       | 否        | false   | **仅一次性采集配置**：配置更新且 inputs 哈希未变时是否强制重新跑任务。 |
| inputs                           | \[object\] | 是        | /       | 输入插件列表。当包含 `input_file`、`input_container_stdio` 或 `input_static_file_onetime` 等单例输入时，仅允许 1 个输入；其它组合需满足「原生与扩展 Input 不可混用」等规则，以加载报错为准。           |
| processors                       | \[object\] | 否        | 空       | 处理插件列表。                         |
| aggregators                      | \[object\] | 否        | 空       | 聚合插件列表。目前每个采集配置最多 1 个聚合插件，且仅在与 Go 投递路径组合时允许，详见 `CollectionConfig::Parse`。 |
| flushers                         | \[object\] | 是        | /       | 输出插件列表，至少 1 个。原生多 Flusher 分发时可在各 Flusher 上配置 `Match`（路由），非顶层字段。            |
| extensions                       | \[object\] | 否        | 空       | 扩展插件列表。                         |

其中，`inputs`、`processors`、`aggregators`、`flushers` 和 `extensions` 中可挂载任意多个[插件](../plugins/overview.md)。

说明：`global` 里列于 `GlobalConfig::sNativeParam` 的键不会进入 `extendedParams`；其余键会并入传给 Go 插件的 `global`（字段名与结构体定义以 `GlobalConfig::Init` + `AddExtendedGlobalParamToGoPipeline` 为准）。

## 组织形式与热加载

默认同级目录下**每个文件对应一条持续采集配置**；文件名（不含扩展名）通常用作采集配置名称标识。

持续采集配置定义所在目录（默认可监听 `continuous_pipeline_config/local` 等来源）支持热加载：新增或修改文件后，LoongCollector 会感知并重新加载；最长等待时间默认约 **10 秒**，可通过实例级参数 **`config_scan_interval`**（见 [系统参数](system-config.md)）调整。

一次性采集配置的加载与生命周期策略以具体发行版与输入类型（如 `input_static_file_onetime`）为准，文件仍落在 `onetime_pipeline_config/local`（及对应 `remote/`）下。

## 示例

一条典型的持续采集配置如下：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/reg.log
processors:
  - Type: processor_parse_regex_native
    SourceKey: content
    Regex: (\d*-\d*-\d*)\s(.*)
    Keys:
      - time
      - msg
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

更多场景可参考仓库 [`example_config`](https://github.com/alibaba/loongcollector/tree/main/example_config)。

* 表 1：Tag 相关项及默认值
|  **配置项**  | **是否默认添加** |  **默认值**  |
| --- | --- | --- |
| HOST_NAME | 是 | **hostname** |
| HOST_IP | 是（开源版） | **host_ip** |
| HOST_ID | 是（企业版） | **host_id** |
| CLOUD_PROVIDER | 是（企业版） | **cloud_provider** |
| AGENT_TAG | 是（企业版） | **agent_tag** |
