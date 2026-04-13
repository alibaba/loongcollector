# Kubernetes元信息采集

## 简介

`service_kubernetes_meta` 定时采集 Kubernetes 元数据，包括 Pod、Deployment 等内置资源及其关系；可通过 **`CustomResources`** 扩展采集第三方 CR（如 Argo Workflow），并生成对应实体与链路日志。提供 HTTP 查询接口，支持通过 Pod IP、Host IP 等索引快速查询元数据。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.0.5 及以上

## 配置参数

**注意：** 本插件需要在 Kubernetes 集群中（或具备访问 apiserver 的配置）运行，且需要有访问Kubernetes API的权限。并且部署模式为单例模式，且配置环境变量 `DEPLOY_MODE=singleton`、`ENABLE_KUBERNETES_META=true`。

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | string，无默认值（必填） | 插件类型，固定为`service_kubernetes_meta`。 |
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
| EnableLabels | bool, false | 是否采集**内置**Kubernetes 资源的标签（Labels）。 |
| EnableAnnotations | bool, false | 是否采集**内置**资源的注解（Annotations）。 |
| NamespaceBlackList | []string，可选 | 全局命名空间过滤：名单内命名空间的对象**不进入 meta 缓存、不投递实体/链路事件**。`Node`、`PersistentVolume`、`StorageClass` 不按命名空间过滤。与 `NamespaceWhiteList` 同时配置时，命名空间允许条件为「未命中黑名单 **或** 在白名单中」（并集语义）；|
| NamespaceWhiteList | []string，可选 | 全局命名空间白名单，见上。均为空则不做命名空间限制。 |
| CustomResources | []object，可选 | 第三方 CR（动态 Informer）采集与链路，见下文「**第三方自定义资源（CustomResources）**」与「**Kubernetes RBAC 权限**」。 |
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


## Kubernetes RBAC 权限

本插件通过 **ServiceAccount**（或 kubeconfig 身份）访问 kube-apiserver。除内置资源外，凡在配置中开启的采集与 **CustomResources** 中声明的 GVR，都需要在 **`ClusterRole`（推荐集群级采集）+ `ClusterRoleBinding`** 或对应的 **`Role` + `RoleBinding`** 中授予至少 **`get`、`list`、`watch`**。

### 常见错误

动态 Informer 对 CR `list/watch` 失败时，日志中可能出现类似：

`cannot list resource "workflows" in API group "argoproj.io" ... User "system:serviceaccount:..." cannot list ...`

说明当前身份**缺少该 API 组下对应复数资源**的权限，与 LoongCollector 配置无关，需在 RBAC 中补齐。

### 内置资源

与配置里打开的内置开关对应即可（如 `pods`、`namespaces`、`deployments` 等）。权限请保持最小可用**`get`、`list`、`watch`** 权限，下面是示例。
```yaml
  - apiGroups: [""]
    resources:
      - configmaps
      - nodes
      - pods
      - services
      - persistentvolumeclaims
      - persistentvolumes
      - namespaces
      - endpoints
    verbs: ["get", "list", "watch"]
  - apiGroups: ["extensions"]
    resources:
      - daemonsets
      - deployments
      - replicasets
      - ingresses
    verbs: ["get", "list", "watch"]
  - apiGroups: ["apps"]
    resources:
      - daemonsets
      - deployments
      - replicasets
      - statefulsets
    verbs: ["get", "list", "watch"]
  - apiGroups: ["batch"]
    resources:
      - cronjobs
      - jobs
    verbs: ["get", "list", "watch"]
  - apiGroups:
      - networking.k8s.io
    resources:
      - ingresses
      - networkpolicies
    verbs:
      - get
      - list
      - watch
  - apiGroups:
      - storage.k8s.io
    resources:
      - storageclasses
      - volumeattachments
    verbs:
      - get
      - list
      - watch
```

### 自定义资源示例：Argo Workflow

采集 `argoproj.io/v1alpha1` 的 `Workflow`（复数资源名 **`workflows`**）时，在 **ClusterRole** 的 `rules` 中增加（按需合并到现有角色）：

```yaml
  - apiGroups:
      - argoproj.io
    resources:
      - workflows
    verbs:
      - get
      - list
      - watch
```

* **`resources`** 须为 CRD 中 **`spec.names.plural`**，可用 `kubectl api-resources --api-group=argoproj.io` 核对。
* 使用 **ClusterRoleBinding** 绑定到运行采集的 ServiceAccount 时，可在全命名空间内 `list/watch` 该资源（Workflow 一般为命名空间作用域资源）。
* 若还需采集同组其它 CR（如 `workflowtemplates`），在 `resources` 列表中继续追加即可。
* 不推荐在生产中用 `resources: ["*"]` 放宽整组权限，除非有明确运维规范。

应用后等待 Informer 重试或重启相关 Pod 使权限生效。


## 第三方自定义资源（CustomResources）

`CustomResources` 为数组，每一项对应一种 CR（**GVR + Kind**），由动态客户端监听；可选生成实体日志及 **Pod→CR**、**Namespace→CR** 链路。

与内置资源相比，CR 在代码里走 **`crUnifiedCache`（dynamic informer + `unstructured`）**，内置走 **`k8sMetaCache`（typed informer）**；二者均实现 **`MetaCache`** 并共用 **`DeferredDeletionMetaStore`**，差异主要在客户端与启动时机。对比如下（**供开发与排障参考**）。

### 内置缓存与 CR 缓存对比

| 维度 | `k8sMetaCache`（内置资源） | `crUnifiedCache`（CustomResources） |
|------|---------------------------|-------------------------------------|
| **K8s 客户端** | `kubernetes.Clientset` | `dynamic.Interface`（`dynamic.NewForConfig`） |
| **Informer** | `informers.SharedInformerFactory` + 各资源 Typed Informer | `dynamicinformer.DynamicSharedInformerFactory` + `ForResource(GVR).Informer()` |
| **资源标识** | 内部 `resourceType` 常量 + `getFactoryInformer` 分支 | `schema.GroupVersionResource`（GVR），`resourceType` 多为配置中的 `EntityType` |
| **对象形态** | 具体 API 类型（如 `*v1.Pod`），经 `preProcess` 等处理 | 统一 `*unstructured.Unstructured`，`objectToUnstructured` |
| **REST / 内容协商** | 跟随 `MetaManager` 为 clientset 设置的配置（含 protobuf 等） | `restConfigForDynamicClient`：**Dynamic 使用 JSON**，与 clientset 的 protobuf 区分 |
| **何时起 watch** | `init(clientset)` 内：`metaStore.Start()` + `watch()` | `init` 不占 clientset；`setRESTConfig` 后由 `EnsureWatchStarted()`（`sync.Once`）**延迟启动** |
| **`watch` 方法** | 完整实现（factory、事件、`WaitForCacheSync` 等） | 空实现；逻辑在 `EnsureWatchStarted` 内 |
| **索引** | `getIdxRules(resourceType)`（如 Host IP 等） | `generateCommonKey` 等 CR 侧规则 |
| **体积优化** | 按资源类型的 `preProcess` | 如 `trimCRObjectForCache`（裁剪 `spec`、`managedFields` 等） |
| **与 `MetaManager.Init` 顺序 init** | 各内置 cache 的 `watch` 参与**顺序**初始化链 | `init` 轻量；避免 GVR / REST 未就绪即起 Informer |

### 子项字段

| 字段 | 类型 | 说明 |
| - | - | - |
| EntityType | string，**必填** | 内部缓存与事件类型键，用于 `__entity_type__`、实体 ID 及链路类型（如 `pod-><EntityType>`）。须在多条 CR 配置间唯一。 |
| APIGroup | string，必填 | CRD 的 API 组，如 `argoproj.io`。 |
| APIVersion | string，必填 | 版本，如 `v1alpha1`。 |
| Resource | string，必填 | **复数**资源名，如 `workflows`（与 `kubectl api-resources` / CRD `spec.names.plural` 一致）。 |
| Kind | string，必填 | Kubernetes **Kind**，如 `Workflow`；用于 `ownerReferences` 匹配及实体日志中的 `kind` 等。 |
| CollectEntity | bool | 为 true 时投递该 CR 的实体（entity）日志。 |
| PodLink | object，可选 | 配置后，在开启 **Pod** 的前提下可生成 **Pod→CR** 链路；需同时配置 **`Entity2PodRelation`**。 |
| Entity2PodRelation | string，可选 | entity_link 中 **`__relation_type__`**（CR 与 Pod 之间的业务关系名）；与 `PodLink` 同时非空时生效。 |
| Namespace2EntityRelation | string，可选 | entity_link 中 **`__relation_type__`**（Namespace 与该 CR）；需 **`CollectEntity: true`**、顶层 **`Namespace: true`** 且本字段非空；仅**有命名空间**的 CR 会生成（集群级 CR 跳过）。 |
| EnableLabels | bool | 为 true 且 `LabelAllowList` 为空时导出**全部** labels；不受顶层 `EnableLabels` 影响。默认不导出。 |
| EnableAnnotations | bool | 同上，作用于 annotations。 |
| LabelAllowList | []string | 非空时仅导出所列 label 键（与 `EnableLabels` 组合见实现逻辑）。 |
| AnnotationAllowList | []string | 非空时仅导出所列 annotation 键。 |
| StatusPathAllowList | []string | 可选，实体日志中 `status` 字段的白名单路径。 |

**PodLink** 子字段：

| 字段 | 说明 |
| - | - |
| OwnerKind | 匹配 Pod `ownerReferences[].Kind`，默认可与条目的 `Kind` 一致。 |
| OwnerAPIGroupContains | 与 `ownerReferences[].apiVersion` 做子串匹配；空则默认使用条目的 `APIGroup`。 |
| PodLabelKey | 无匹配 owner 时的回退标签，如 Argo 的 `workflows.argoproj.io/workflow`。 |

### 配置示例：Argo Workflow

```yaml
enable: true
inputs:
  - Type: service_kubernetes_meta
    Interval: 600
    Node: true
    Pod: true
    NamespaceBlackList:
      - kube-system  
    # NamespaceWhiteList:
    #   - default
    #   - production
    # Third-party CRs: configure each GVR (and optional PodLink / Entity2PodRelation) under CustomResources.
    CustomResources:
      - APIGroup: argoproj.io     # API-Group
        APIVersion: v1alpha1       # API version
        Resource: workflows       # 资源类型
        Kind: Workflow                 # kind信息
        EntityType: argo.workflow    #实体类型
        CollectEntity: true
        Entity2PodRelation: "contains" 
        Namespace2EntityRelation: "contains"     
        PodLink:  # 和Pod的关联关系提取
          OwnerKind: Workflow          # Pod OwnerReference  对应的资源Kind
          OwnerAPIGroupContains: argoproj.io     # Pod OwnerReference 对应的 API-Group
          PodLabelKey: workflows.argoproj.io/workflow  #  兜底逻辑，从label中提取关联关系
        EnableLabels: true        # CR粒度配置是否上报标签
        EnableAnnotations: true # CR粒度配置是否上报注释
```

同时请确保 **RBAC** 已授予对 `argoproj.io` / `workflows` 的 `get、list、watch`（见上文「Kubernetes RBAC 权限」）。


## 环境变量

如需使用HTTP查询接口，需要配置环境变量`KUBERNETES_METADATA_PORT`，指定HTTP查询接口的端口号。

## 样例

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_kubernetes_meta
    Pod: true
    EnableLabels: true
    EnableAnnotations: true
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
  "__entity_type__":"k8s.pod",
  "labels":"{\"app\":\"flannel\",\"tier\":\"node\"}",
  "annotations":"{\"description\":\"Flannel network plugin\"}",
  "__pack_meta__":"1|MTcyMjQ4NDQ3MzA5MTA3Njc1OQ==|47|21",
  "__time__":"1723276913"
}
```

## CR 开发说明

**实现上内置与 CR 的差异**见上文「**第三方自定义资源（CustomResources）**」中的 **「内置缓存与 CR 缓存对比」** 表；源码分别位于 `pkg/helper/k8smeta/k8s_meta_cache.go` 与 `k8s_meta_cr_unified_cache.go`。

### 扩展 CR 时可关注的代码路径

* **配置与注册**：`pkg/helper/k8smeta/k8s_meta_custom_resource.go`、`MetaManager.RegisterCustomResourceCollector`（`k8s_meta_manager.go`）。
* **采集与投递**：`plugins/input/kubernetesmetav2/meta_collector.go` 中对 `EntityType` / 链路类型的处理。
* **命名空间策略**：`k8s_meta_namespace_policy.go`（`ObjectMetaNamespaceForFilter` 对 `unstructured` 与内置类型均已覆盖）。

新增 CR 时一般只需**配置层**声明 GVR 与链路字段；若需新索引、新裁剪或新事件语义，再在 **`crUnifiedCache`** 与 **`meta_collector`** 侧按现有模式扩展即可。
