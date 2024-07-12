# GPU 指标

## 简介

`service_gpu_metric` `input`插件可以采集 英伟达 GPU 相关指标（如未安装英伟达驱动，此插件无法正常工作）。

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| --- |--------| --- |
| CollectIntervalMs | int(1000)  | 插件类型，指定为`service_gpu_metric`。 |

## 样例

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_gpu_metric
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出

```json
{
    "metric_type":"gpu",
    "device":"0",
    "gpu_power_usage":"7",
    "gpu_temperature":"36",
    "gpu_util":"0",
    "gpu_total_memory":"7611",
    "gpu_free_memory":"7611",
    "gpu_memory_util":"0",
    "gpu_used_memory":"0",
    "__time__":"1663034534"
}
```

## 采集指标含义

| 名称                | 说明                           |
|-------------------|------------------------------|
| gpu_power_usage | 此 GPU 及其相关电路的电源使用情况(以毫瓦为单位)。 |
| gpu_temperature | 此 GPU 的温度（以摄氏度为单位）。          |
| gpu_util | GPU 使用率。                     |
| gpu_memory_util | GPU 内存使用率。                   |
| gpu_used_memory | GPU 使用内存(MB)。                |
| gpu_total_memory | GPU 总内存(MB)。                 |
| gpu_free_memory | GPU 剩余内存(MB)。                |
