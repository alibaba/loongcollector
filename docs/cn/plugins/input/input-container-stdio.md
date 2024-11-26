# 容器标准输出（原生插件）

## 简介

`input_container_stdio` `input`插件可以实现从容器标准输出/标准错误流中采集日志，采集的日志内容将会保存在`content`字段中。支持通过容器元信息筛选待采集容器，并支持多行文本切分、添加容器Meta信息等数据处理操作，仅支持 iLogtail 2.1 及以后版本。

## 版本

[Stable](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_container\_log。  |
|  IgnoringStdout  |  Boolean  |  否  |  false  |  是否忽略标准输出stdout。  |
|  IgnoringStderr  |  Boolean  |  否  |  false  |  是否忽略标准出错信息stderr。 |
|  TailSizeKB  |  uint  |  否  |  1024  |  配置首次生效时，标准输出文件的起始采集位置距离文件结尾的大小。如果文件大小小于该值，则从头开始采集，取值范围为0～10485760KB。  |
|  Multiline  |  object  |  否  |  空  |  多行聚合选项，详见表1。  |
|  ContainerFilters  |  object  |  否  |  空  |  容器过滤选项。多个选项之间为“且”的关系，详见表2。  |
|  ExternalK8sLabelTag  |  map  |  否  |  空  |  对于部署于K8s环境的容器，需要在日志中额外添加的与Pod标签相关的tag。map中的key为Pod标签名，value为对应的tag名。 例如：在map中添加`app: k8s_label_app`，则若pod中包含`app=serviceA`的标签时，会将该信息以tag的形式添加到日志中，即添加字段\_\_tag\_\_:k8s\_label\_app: serviceA；若不包含`app`标签，则会添加空字段\_\_tag\_\_:k8s\_label\_app:  |
|  ExternalEnvTag  |  map  |  否  |  空  |  对于部署于K8s环境的容器，需要在日志中额外添加的与容器环境变量相关的tag。map中的key为环境变量名，value为对应的tag名。 例如：在map中添加`VERSION: env_version`，则当容器中包含环境变量`VERSION=v1.0.0`时，会将该信息以tag的形式添加到日志中，即添加字段\_\_tag\_\_:env\_version: v1.0.0；若不包含`VERSION`环境变量，则会添加空字段\_\_tag\_\_:env\_version:  |
|  FlushTimeoutSecs  |  uint  |  否  |  5  |  当文件超过指定时间未出现新的完整日志时，将当前读取缓存中的内容作为一条日志输出。  |
|  AllowingIncludedByMultiConfigs  |  bool  |  否  |  false  |  是否允许当前配置采集其它配置已匹配的容器的标准输出日志。  |

* 表1：多行聚合选项

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Mode  |  string  |  否  |  custom  |  多行聚合模式。仅支持custom。  |
|  StartPattern  |  string  |  当Multiline.Mode取值为custom时，至少1个必填  |  空  |  行首正则表达式。  |
|  ContinuePattern  |  string  |  |  空  |  行继续正则表达式。  |
|  EndPattern  |  string  |  |  空  |  行尾正则表达式。  |
|  UnmatchedContentTreatment  |  string  |  否  |  single_line  |  对于无法匹配的日志段的处理方式，可选值如下：<ul><li>discard：丢弃</li><li>single_line：将不匹配日志段的每一行各自存放在一个单独的事件中</li></ul>   |

* 表2：容器过滤选项

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  K8sNamespaceRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在Pod所属的命名空间条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  K8sPodRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在Pod的名称条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  IncludeK8sLabel  |  map  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在pod的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为Pod标签名，value为Pod标签的值，说明如下：<ul><li>如果map中的value为空，则pod标签中包含以key为键的pod都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当pod标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的pod会被匹配；</li><li>其他情况下，当pod标签中存在以key为标签名且以value为标签值的情况时，相应的pod会被匹配。</li></ul></ul>       |
|  ExcludeK8sLabel  |  map  |  否  |  空  |  对于部署于K8s环境的容器，指定需要排除采集容器所在pod的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为pod标签名，value为pod标签的值，说明如下：<ul><li>如果map中的value为空，则pod标签中包含以key为键的pod都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当pod标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的pod会被匹配；</li><li>其他情况下，当pod标签中存在以key为标签名且以value为标签值的情况时，相应的pod会被匹配。</li></ul></ul>       |
|  K8sContainerRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器的名称条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  IncludeEnv  |  map  |  否  |  空  |  指定待采集容器的环境变量条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为环境变量名，value为环境变量的值，说明如下：<ul><li>如果map中的value为空，则容器环境变量中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器环境变量中存在以key为环境变量名且对应环境变量值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器环境变量中存在以key为环境变量名且以value为环境变量值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  ExcludeEnv  |  map  |  否  |  空  |  指定需要排除采集容器的环境变量条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为环境变量名，value为环境变量的值，说明如下：<ul><li>如果map中的value为空，则容器环境变量中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器环境变量中存在以key为环境变量名且对应环境变量值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器环境变量中存在以key为环境变量名且以value为环境变量值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  IncludeContainerLabel  |  map  |  否  |  空  |  指定待采集容器的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则默认为空，表示采集所有容器。支持正则匹配。 map中的key为容器标签名，value为容器标签的值，说明如下：<ul><li>如果map中的value为空，则容器标签中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器标签中存在以key为标签名且以value为标签值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  ExcludeContainerLabel  |  map  |  否  |  空  |  指定需要排除采集容器的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则默认为空，表示采集所有容器。支持正则匹配。 map中的key为容器标签名，value为容器标签的值，说明如下：<ul><li>如果map中的value为空，则容器标签中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器标签中存在以key为标签名且以value为标签值的情况时，相应的容器会被匹配。</li></ul></ul>       |

## 默认日志字段

所有使用本插件上报的日志均额外携带下列字段。目前暂不支持更改。

| 字段名称                | 说明                                         |
| ------------------- | ------------------------------------------ |
| \_time\_            | 日志采集时间，例如`2021-02-02T02:18:41.979147844Z`。 |
| \_source\_          | 日志源类型，stdout或stderr。                       |

## 默认Tag字段

所有使用本插件上报的日志Tag均额外携带下列字段。目前暂不支持更改。

| 字段名称                | 说明                                         |
| ------------------- | ------------------------------------------ |
| \_image\_name\_     | 镜像名                                        |
| \_container\_name\_ | 容器名                                        |
| \_pod\_name\_       | Pod名                                       |
| \_namespace\_       | Pod所在的命名空间                                 |
| \_pod\_uid\_        | Pod的唯一标识                                   |


## 样例

### 示例1：通过容器环境变量黑白名单过滤容器 <a href="#h3-url-1" id="h3-url-1"></a>

采集环境变量含`NGINX_SERVICE_PORT=80`且不含`POD_NAMESPACE=kube-system`的容器的标准输出。

1\. 获取环境变量。

您可以登录容器所在的宿主机查看容器的环境变量。具体操作，请参见[获取容器环境变量](https://help.aliyun.com/document\_detail/354831.htm#section-0a0-n4l-zhi)。

{% hint style="info" %}
`命令提示：`

docker inspect

crictl inspect

ctr -n k8s.io containers info
{% endhint %}

```plain
            "Env": [
                ...
                "NGINX_SERVICE_PORT=80",
```

2\. 创建iLogtail采集配置。

iLogtail采集配置示例如下所示。

```yaml
inputs:
  - Type: input_container_stdio
    IgnoringStdout: false
    IgnoringStderr: true
    ContainerFilters:
        IncludeEnv:
            NGINX_SERVICE_PORT: "80"
        ExcludeEnv:
            POD_NAMESPACE: "kube-system"
```

### 示例2：通过容器Label黑白名单过滤容器 <a href="#h3-url-2" id="h3-url-2"></a>

采集容器Label含`io.kubernetes.container.name:nginx`且不含`io.kubernetes.pod.namespace:kube-system`容器的标准输出。

1\. 获取容器Label。

您可以登录容器所在的宿主机查看容器的Label。具体操作，请参见[获取容器Label](https://help.aliyun.com/document\_detail/354831.htm#section-7rp-xn8-crg)。

```plain
            "Labels": {
                ...
                "io.kubernetes.container.name": "nginx",
```

2\. 创建Logtail采集配置。

Logtail采集配置示例如下所示。

```yaml
inputs:
  - Type: input_container_stdio
    IgnoringStdout: false
    IgnoringStderr: true
    ContainerFilters:
        IncludeContainerLabel:
          io.kubernetes.container.name: "nginx"
        IncludeContainerLabel:
          io.kubernetes.pod.namespace": "kube-system"
```

### 示例3：通过Kubernetes Namespace名称、Pod名称和容器名称过滤容器 <a href="#h3-url-3" id="h3-url-3"></a>

采集default命名空间下以nginx-开头的Pod中的nginx容器的标准输出。

1\. 获取Kubernetes层级的信息。

{% hint style="info" %}
`命令提示：`

kubectl describe
{% endhint %}

```plain
Name:         nginx-76d49876c7-r892w
Namespace:    default
...
Containers:
  nginx:
    Container ID:...
```

2\. 创建Logtail采集配置。 Logtail采集配置示例如下所示。

```yaml
inputs:
  - Type: input_container_stdio
    IgnoringStdout: false
    IgnoringStderr: true
    ContainerFilters:
        K8sNamespaceRegex: "^(default)$"
        K8sPodRegex: "^(nginx-.*)$"
        K8sContainerRegex: "^(nginx)$"
```

### 示例4：通过Kubernetes Label过滤容器 <a href="#h3-url-4" id="h3-url-4"></a>

采集Kubernetes Label含app=nginx且不含env=test开头的所有容器的标准输出。

1\. 获取Kubernetes层级的信息。

```plain
Name:         nginx-76d49876c7-r892w
Namespace:    default
...
Labels:       app=nginx
              ...
```

2\. 创建Logtail采集配置。 Logtail采集配置示例如下所示。

```yaml
inputs:
  - Type: input_container_stdio
    IgnoringStdout: false
    IgnoringStderr: true
    ContainerFilters:
        IncludeK8sLabel:
          app: "nginx"
        ExcludeK8sLabel:
          env: "^(test.*)$"
```

### 示例5：多行日志的iLogtail采集配置 <a href="#title-asn-vuf-17z" id="title-asn-vuf-17z"></a>

采集输出在标准错误流的Java异常堆栈（多行日志）。

1\. 获取日志样例。

```plain
2021-02-03 14:18:41.969 ERROR [spring-cloud-monitor] [nio-8080-exec-4] c.g.s.web.controller.DemoController : java.lang.NullPointerException
at org.apache.catalina.core.ApplicationFilterChain.internalDoFilter(ApplicationFilterChain.java:193)
at org.apache.catalina.core.ApplicationFilterChain.doFilter(ApplicationFilterChain.java:166)
...
```

&#x20;   根据有区分度的行首编写正则。

&#x20;   行首：`2021-02-03 ...`

&#x20;   正则：`\d+-\d+-\d+.*`

2\. 创建Logtail采集配置。 Logtail采集配置示例如下所示。

```yaml
inputs:
  - Type: input_container_stdio
    IgnoringStdout: false
    IgnoringStderr: true
    Multiline:
      StartPattern: \d+-\d+-\d+.*
```
