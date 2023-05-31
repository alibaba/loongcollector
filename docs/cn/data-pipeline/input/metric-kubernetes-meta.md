# Kubernetes Meta

## 简介

`metric_meta_kubernetes` `input`插件可以采集 Kubernetes 资源信息元数据。

## 配置参数

### 全局参数

| 参数                    | 类型，默认值                | 说明                                                      |
|-----------------------|-----------------------|---------------------------------------------------------|
| Pod                   | bool,true             | 收集 Pod 元信息                                              |   
| Node                  | bool,true             | 收集 Node 元信息                                             |   
| Service               | bool,true             | 收集 Service 元信息                                          |   
| Deployment            | bool,true             | 收集 Deployment 元信息                                       |   
| DaemonSet             | bool,true             | 收集 DaemonSet 元信息                                        |   
| StatefulSet           | bool,true             | 收集 StatefulSet 元信息                                      |   
| Configmap             | bool,true             | 收集 Configmap 元信息                                        |   
| Secret                | bool,true             | 收集 Secret 元信息                                           |   
| Job                   | bool,true             | 收集 Job 元信息                                              |   
| CronJob               | bool,true             | 收集 CronJob 元信息                                          |   
| Namespace             | bool,true             | 收集 Namespace 元信息                                        |   
| PersistentVolume      | bool,true             | 收集 PersistentVolume 元信息                                 |   
| PersistentVolumeClaim | bool,true             | 收集 PersistentVolumeClaim 元信息                            |   
| StorageClass          | bool,true             | 收集 StorageClass 元信息                                     |   
| Ingress               | bool,true             | 收集 Ingress 元信息                                          |   
| DisableReportParents  | bool,false            | 关闭收集父信息关联上报，如Deployment 产生Pod，则父信息为Deployment           |
| KubeConfigPath        | string,""             | 测试使用，配置KubeConfig 地址                                    |
| SelectedNamespaces    | []string,[]           | 收集资源信息的范围，默认是全部                                         |
| LabelSelectors        | string,""             | 支持按 k8s.io/apimachinery/pkg/labels/selector.go 规则配置标签筛选 |
| IntervalMs            | int,300000            | 每次收集的周期，默认300s                                          |
| EnableOpenKruise      | bool,false            | 开启收集Kruise 增强Kubernetes资源信息                             |
| Labels                | map[string]string ,{} | 支持为元信息增加Labels                                          |

## 样例


### 采集kubernetes 元信息

```yaml
    inputs:
      - Type: metric_meta_kubernetes
        Labels:
          global: outer

```

2. 采集结果

```text
2023-05-28 02:49:44 [INF] [flusher_stdout.go:120] [Flush] [##1.0##qs-demos$sls-mall__k8s-metas__sls-mall,sls-mall-metas]	{"id":"27b7d494-2135-479e-be6a-6e192da849d3","type":"Pod","attributes":"{\"restart_count\":0,\"name\":\"golang-pull-6b946f8f6c-89kcl\",\"pod_ip\":\"\",\"container.0.image_name\":\"registry-vpc.cn-beijing.aliyuncs.com/log-service/logtail:profile-go-pull\",\"creation_time\":1681760673,\"phase\":\"Failed\",\"volume_claim\":\"\",\"resource_version\":\"377975647\",\"namespace\":\"profile\",\"workload\":\"golang-pull\",\"container.0.container_name\":\"golang-pull\"}","labels":"{\"app\":\"golang-pull\",\"pod-template-hash\":\"6b946f8f6c\",\"cluster\":\"sls-mall\"}","parents":"[\"Node:011574fd-110e-4705-bd3b-215e2bba6609:cn-beijing.192.168.32.120\",\"Deployment:3b9f3055-b12d-4c0b-b4ce-38dbb9ca832c:golang-pull\"]","__time__":"1685242184"}
```