# 元标签添加原生处理插件

## 简介

`processor_tag_native` 插件为事件组添加主机/环境等元标签，支持通过配置重命名或选择性追加环境变量标签。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  固定为 `processor_tag_native`。 |
|  PipelineMetaTagKey | map[string]string | 否 | 见说明 | 重命名管线级元标签键名。常见键：`HOST_NAME`（社区版还包括 `HOST_IP`），企业版还支持 `HOST_ID`、`CLOUD_PROVIDER`、`AGENT_TAG`。 |
|  AgentEnvMetaTagKey | map[string]string | 否 | 空 | 选择性追加环境变量标签：键为环境变量名，值为在事件中的目标标签名（为空表示不追加）。未配置时默认追加所有可配置环境标签。 |

## 产出标签（示例）

- 社区版：`__hostname__`、`__source__`（IP）、`machine_uuid` 等
- 企业版：还包括 `__host_id__`、`__cloud_provider__`、`__agent_tag__` 等


