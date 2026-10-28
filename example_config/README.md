# Example Config Directory

## About

本目录存放 **LoongCollector** 的示例配置与入门脚手架（曾用名 iLogtail 的配置示例亦多迁移至此）。

- `quick_start`、`start_with_docker`、`start_with_k8s`：端到端入门示例，可与中文安装文档对照使用。
- `data_pipelines`：按场景拆分的流水线片段，**默认使用 YAML**，突出 processor / aggregator 等组合；命名约定见下文贡献说明。
- `user_contrib`：社区贡献的采集模版草稿或说明。

## 重要说明（与文档维护约定一致）

1. **本目录不是「权威规格」**：插件参数、默认值、合法组合以**仍在构建与测试中的源码**及 **E2E / 单测** 加载的配置为准。复制示例到生产前请对照当前版本文档或代码。
2. **流水线配置推荐使用 YAML**：与社区文档及新 PR 默认保持一致；若发现与实现不符的示例，应修正示例或补充测试，避免仅改文档。
3. **`loongcollector_config.json` 等进程级文件** 仍为 JSON，这是产品行为，与「采集 YAML」不矛盾。

## 贡献指南（节选）

`example_config/data_pipelines` 目录下模版以 `<场景-关键插件>.yaml` 命名，例如 `nginx-regex.yaml`、`file-delimiter.yaml`。模版内注释需说明适用场景及部署前必须替换的占位项。

原则上模版应精简、正交，便于组合。若不需强调 input/flusher，可使用通用 `input_file` / `flusher_stdout`，避免无意义的重复示例。

提交模版时，Issue / PR 建议打上 **`example config`** 标签。
