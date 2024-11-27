# Kubernetes元信息采集

## 简介

`service_kubernetes_meta` 定时采集Kubernetes元数据，包括Pod、Deployment等资源及其之间的关系。并提供HTTP查询接口，支持通过一些字段索引，如Pod IP、Host IP等信息快速查询元数据。

## 版本

[Beta](../stability-level.md)

## 配置参数

**注意：** 本插件需要在Kubernetes集群中运行，且需要有访问Kubernetes API的权限。并且部署模式为单例模式，且配置环境变量`DEPLOY_MODE`为`singleton`，`ENABLE_KUBERNETES_META`为`true`。

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`service_kubernetes_meta`。 |
| Interval | int, 30 | 采集间隔时间，单位为秒。 |
| Pod | bool, false | 是否采集Pod元数据。 |
| Node | bool, false | 是否采集Node元数据。 |
| Service | bool, false | 是否采集Service元数据。 |
| Deployment | bool, false | 是否采集Deployment元数据。 |
| DaemonSet | bool, false | 是否采集DaemonSet元数据。 |
| StatefulSet | bool, false | 是否采集StatefulSet元数据。 |
| Configmap | bool, false | 是否采集ConfigMap元数据。 |
| Job | bool, false | 是否采集Job元数据。 |
| CronJob | bool, false | 是否采集CronJob元数据。 |
| Namespace | bool, false | 是否采集Namespace元数据。 |
| PersistentVolume | bool, false | 是否采集PersistentVolume元数据。 |
| PersistentVolumeClaim | bool, false | 是否采集PersistentVolumeClaim元数据。 |
| StorageClass | bool, false | 是否采集StorageClass元数据。 |
| Ingress | bool, false | 是否采集Ingress元数据。 |

## 环境变量

如需使用HTTP查询接口，需要配置环境变量`KUBERNETES_METADATA_PORT`，指定HTTP查询接口的端口号。

## 样例

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_kubernetes_meta
    Pod: true
```

* 输出

```json
{
  "__method__":"update",
  "__first_observed_time__":"1723276582",
  "__last_observed_time__":"1723276582",
  "__keep_alive_seconds__":"3600",
  "__category__":"entity",
  "__domain__":"k8s","__entity_id__":"38a8cc4e856ec7d5b2675868411f696f053dccebc06b8819b02442ee5a07091c",
  "namespace":"kube-system",
  "name":"kube-flannel-ds-zh5fx",
  "__entity_type__":"pod",
  "__pack_meta__":"1|MTcyMjQ4NDQ3MzA5MTA3Njc1OQ==|47|21",
  "__time__":"1723276913"
}
```
