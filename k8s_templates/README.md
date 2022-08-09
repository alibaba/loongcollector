# The K8s Templates Directory

## About

This folder contains out-of-box example configuration templates for iLogtail K8s deployment. It highlights the usage of common input and flusher plugins in the K8s environment.

Please feel free to add one if it doesn't cover your case.

## 贡献指南

`k8s_templates`目录包含完整的K8s部署案例，从源到典型处理到输出，以input / flusher插件进行划分。模版以`k8s-daemonset-<source>-to-<destination>.yaml`命名，例如`k8s-daemonset-stdout-to-sls.yaml`，`k8s-daemonset-file-to-kafka.yaml`，统一存放在`k8s_templates`目录下。

模版内注释需要说明清楚适用场景和应用前需要替换的部分。

原则上模版应经量全面精简正交，便于用户理解后查找，自由组合。source为主的模版除 `file_log` 和 `service_docker_stdout` 插件具备kafka和sls 2个flusher外，其余都统一使用 `flusher_sls` 作为flusher。flusher为主的模版除 `flusher_kafka` 和 `flusher_sls` 外，应只选用1个配合最佳的插件作为input（容器场景通常是 `service_docker_stdout` ），避免意义不大的重复建设。
