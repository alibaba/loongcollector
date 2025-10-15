# 主机监控数据（原生插件）

## 简介

`input_host_monitor` 插件定期采集宿主机的 CPU、内存、磁盘、网络、系统负载、进程等监控指标。

## 版本

[Beta](../../stability-level.md)

## 配置参数

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| Type | string | 是 | / | 固定为 `input_host_monitor` |
| Interval | uint | 否 | 15 | 采集周期（秒）。最小 5 秒，小于 5 时按 5 处理。 |
| EnableCPU | bool | 否 | true | 是否启用 CPU 指标采集 |
| EnableSystem | bool | 否 | true | 是否启用系统负载等指标采集 |
| EnableMemory | bool | 否 | true | 是否启用内存指标采集 |
| EnableDisk | bool | 否 | true | 是否启用磁盘指标采集 |
| EnableProcess | bool | 否 | true | 是否启用进程指标采集 |
| EnableNet | bool | 否 | true | 是否启用网络指标采集 |

## 样例

```yaml
inputs:
  - Type: input_host_monitor
    Interval: 10
    EnableCPU: true
    EnableSystem: true
    EnableMemory: true
    EnableDisk: true
    EnableProcess: false
    EnableNet: true
```
