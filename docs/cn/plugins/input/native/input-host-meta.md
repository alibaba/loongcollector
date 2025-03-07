# 主机元信息采集

## 简介

`input_host_meta` 定时采集主机元数据，包括主机、进程及其之间的关系。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`input_host_meta`。 |
| Interval | int, 60 | 采集间隔时间，单位为秒。 |

## 样例

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_host_meta
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
  "__domain__":"infra",
  "__entity_type__":"infra.host.process",
  "__entity_id__":"8a1aee58dcdea68434e058e48e39f965",
  "__first_observed_time__":"1735348941",
  "__last_observed_time__":"1736163039",
  "__keep_alive_seconds__":"120",
  "pid":"84450",
  "ppid":"0",
  "comm":"sh",
  "ktime":"1735348941"
}
```
