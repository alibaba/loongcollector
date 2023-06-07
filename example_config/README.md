# The Example Config Directory

## About

This folder contains example configs for iLogtail.

`quick_start`, `start_with_docker` and `start_with_k8s` are 3 complete basic examples that accord with [Installation Manual](https://ilogtail.gitbook.io/ilogtail-docs/installation).

The `data_pipelines` directory contains more advanced examples for different use cases. Please feel free to add one if it doesn't cover your case.

The `user_contrib` directory is convenient for developers to freely contribute collection templates.


## 贡献指南

`example_config/data_pipelines`目录包含场景化的采集配置模版，重点突出processor/aggregator插件功能。目录下都模版以`<senario-keyplugin>.yaml`命名，例如`nginx-regex.yaml`，`file-delimiter.yaml`。若找不到具体场景可以用笼统的file命名，若要用到多个插件请仅使用一个或多个不可替换的关键插件命名，模版都约定存放在`example_config/data_pipelines`目录下。

模版内注释需要说明清楚适用场景和应用前需要替换的部分。

原则上模版应尽量全面精简正交，便于用户理解后查找，自由组合。模版中若不需要突出input或flusher的使用，直接使用file_log / flusher_stdout即可，避免意义不大的重复建设。

提交模版时，Issue / PR 请打上标签`example config`。
