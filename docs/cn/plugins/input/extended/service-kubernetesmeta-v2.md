# Kubernetes元信息采集

## 简介

`service_kubernetes_meta` 定时采集Kubernetes元数据，包括Pod、Deployment等资源及其之间的关系。并提供HTTP查询接口，支持通过一些字段索引，如Pod IP、Host IP等信息快速查询元数据。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.0.5 及以上

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
| Node2Pod | string, 无默认值（可选） | Node到Pod的关系名，不填则不生成关系。 |
| Deployment2Pod | string, 无默认值（可选） | Deployment到Pod的关系名，不填则不生成关系。 |
| ReplicaSet2Pod | string, 无默认值（可选） | ReplicaSet到Pod的关系名，不填则不生成关系。 |
| Deployment2ReplicaSet | string, 无默认值（可选） | Deployment到ReplicaSet的关系名，不填则不生成关系。 |
| StatefulSet2Pod | string, 无默认值（可选） | StatefulSet到Pod的关系名，不填则不生成关系。 |
| DaemonSet2Pod | string, 无默认值（可选） | DaemonSet到Pod的关系名，不填则不生成关系。 |
| Service2Pod | string, 无默认值（可选） | Service到Pod的关系名，不填则不生成关系。 |
| Pod2Container | string, 无默认值（可选） | Pod到Container的关系名，不填则不生成关系。 |
| CronJob2Job | string, 无默认值（可选） | CronJob到Job的关系名，不填则不生成关系。 |
| Job2Pod | string, 无默认值（可选） | Job到Pod的关系名，不填则不生成关系。 |
| Ingress2Service | string, 无默认值（可选） | Ingress到Service的关系名，不填则不生成关系。 |
| Pod2PersistentVolumeClaim | string, 无默认值（可选） | Pod到PersistentVolumeClaim的关系名，不填则不生成关系。 |
| Pod2Configmap | string, 无默认值（可选） | Pod到Configmap的关系名，不填则不生成关系。 |
| Namespace2Pod | string, 无默认值（可选） | Namespace到Pod的关系名，不填则不生成关系。 |
| Namespace2Service | string, 无默认值（可选） | Namespace到Service的关系名，不填则不生成关系。 |
| Namespace2Deployment | string, 无默认值（可选） | Namespace到Deployment的关系名，不填则不生成关系。 |
| Namespace2DaemonSet | string, 无默认值（可选） | Namespace到DaemonSet的关系名，不填则不生成关系。 |
| Namespace2StatefulSet | string, 无默认值（可选） | Namespace到StatefulSet的关系名，不填则不生成关系。 |
| Namespace2Configmap | string, 无默认值（可选） | Namespace到Configmap的关系名，不填则不生成关系。 |
| Namespace2Job | string, 无默认值（可选） | Namespace到Job的关系名，不填则不生成关系。 |
| Namespace2CronJob | string, 无默认值（可选） | Namespace到CronJob的关系名，不填则不生成关系。 |
| Namespace2PersistentVolumeClaim | string, 无默认值（可选） | Namespace到PersistentVolumeClaim的关系名，不填则不生成关系。 |
| Namespace2Ingress | string, 无默认值（可选） | Namespace到Ingress的关系名，不填则不生成关系。 |
| Cluster2Namespace | string, 无默认值（可选） | Cluster到Namespace的关系名，不填则不生成关系。 |
| Cluster2Node | string, 无默认值（可选） | Cluster到Node的关系名，不填则不生成关系。 |
| Cluster2PersistentVolume | string, 无默认值（可选） | Cluster到PersistentVolume的关系名，不填则不生成关系。 |
| Cluster2StorageClass | string, 无默认值（可选） | Cluster到StorageClass的关系名，不填则不生成关系。 |


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
