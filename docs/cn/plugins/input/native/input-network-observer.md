# input_network_observer 插件

## 简介

`input_network_observer`插件可以实现利用ebpf探针采集网络可观测数据。

## 版本

[Dev](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_network\_observer  |
|  ProbeConfig  |  object  |  是  |  /  |  插件配置参数列表  |
|  ProbeConfig.L7Config  |  object  |  是  |  /  |  Layer7 配置  |
|  ProbeConfig.L7Config.Enable  |  bool  |  否  |  false  |  是否开启  |
|  ProbeConfig.L7Config.SampleRate  |  float  |  否  |  0.1  |  采样率  |
|  ProbeConfig.L7Config.EnableMetric  |  bool  |  否  |  false  |  是否开启指标上报  |
|  ProbeConfig.L7Config.EnableLog  |  bool  |  否  |  false  |  是否开启日志上报  |
|  ProbeConfig.L7Config.EnableSpan  |  bool  |  否  |  false  |  是否开启链路追踪上报  |
|  ProbeConfig.L4Config  |  object  |  是  |  /  |  Layer4 配置  |
|  ProbeConfig.L4Config.Enable  |  bool  |  否  |  false  |  是否开启  |
|  ProbeConfig.ApmConfig  |  object  |  是  |  /  |  应用相关配置  |
|  ProbeConfig.ApmConfig.AppName  |  string  |  是  |  /  |  应用名称  |
|  ProbeConfig.ApmConfig.AppId  |  string  |  是  |  /  |  应用id  |
|  ProbeConfig.ApmConfig.Workspace  |  string  |  是  |  /  |  工作空间  |
|  ProbeConfig.ApmConfig.ServiceId  |  string  |  是  |  /  |  服务id  |
|  ProbeConfig.WorkloadSelectors  |  object  |  否  |  /  |  负载选择器  |
|  ProbeConfig.WorkloadSelectors.Namespace  |  string  |  是  |  /  |  K8s命名空间  |
|  ProbeConfig.WorkloadSelectors.WorkloadKind  |  string  |  是  |  /  |  K8s负载类型：Deployment,Daemonset,StatefulSet ...  |
|  ProbeConfig.WorkloadSelectors.WorkloadName  |  string  |  是  |  /  |  K8s负载名  |

## 样例

### XXXX

* 输入

```json
TODO
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_network_observer
    ProbeConfig:
      L7Config:
        Enable: true
        SampleRate: 0.1
        EnableMetric: false
        EnableSpan: false
        EnableLog: true
      L4Config: 
        Enable: true 
      ApmConfig:
        AppName: test-app-name
        AppId: test-app-id
        Workspace: test-workspace
        ServiceId: test-service-id
      WorkloadSelectors:
        - WorkloadName: test-workload
          Namespace: default
          WorkloadKind: Deployment
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
TODO
```
