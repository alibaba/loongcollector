# 容器标准输出

## 简介

`service_docker_stdout` `input`插件可以实现从容器标准输出/标准错误流中采集日志，采集的日志内容将会保存在`content`字段中。支持通过容器元信息筛选待采集容器，并支持多行文本切分、添加容器Meta信息等数据处理操作，概览如下：

容器筛选，每项筛选条件之间为AND关系

* 支持通过容器Label白名单指定待采集的容器，多个白名单之间关系为OR。
* 支持通过容器Label黑名单排除不要采集的容器，多个黑名单之间关系为OR。
* 支持通过环境变量白名单指定待采集的容器，多个白名单之间关系为OR。
* 支持通过环境变量黑名单排除不要采集的容器，多个黑名单之间关系为OR。
* 支持通过Kubernetes Namespace名称指定待采集的容器。
* 支持通过Kubernetes Pod名称指定待采集的容器。
* 支持通过Kubernetes容器名称指定待采集的容器。
* 支持通过Kubernetes Label白名单指定待采集的容器，多个白名单之间关系为OR。
* 支持通过Kubernetes Label黑名单排除不要采集的容器，多个黑名单之间关系为OR。

数据处理

* 支持采集多行日志（例如Java Stack日志等）。
* 支持上报时自动关联Kubernetes Label信息。
* 支持上报时自动关联容器Meta信息（例如容器名、IP、镜像、Pod、Namespace、环境变量等）。
* 支持上报时自动关联宿主机Meta信息（例如宿主机名、IP、环境变量等）。

## 版本

[Stable](../stability-level.md)

## 配置参数

### 基本参数

| 参数                | 类型      | 是否必选 | 说明                                                                         |
| ----------------- | ------- | ---- | -------------------------------------------------------------------------- |
| Type              | String  | 是    | 插件类型，固定为`service_docker_stdout`                                            |
| Stdout            | Boolean | 否    | 是否采集标准输出stdout。默认取值为`true`。</p>                       |
| Stderr            | Boolean | 否    | 是否采集标准出错信息stderr。默认取值为`true`。</p>                     |
| StartLogMaxOffset | Integer | 否    | 首次采集时回溯历史数据长度，单位：字节。建议取值在[131072,1048576]之间。默认取值为128×1024字节。 |

### 筛选容器参数

| 参数                | 类型                                 | 是否必选 | 说明                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| ----------------- | ---------------------------------- | ---- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| IncludeLabel      | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>容器Label白名单，用于指定待采集的容器。默认为空，表示采集所有容器的标准输出。如果您要设置容器Label白名单，那么LabelKey必填，LabelValue可选填。</p><ul><li>如果LabelValue为空，则容器Label中包含LabelKey的容器都匹配。</li><li><p>如果LabelValue不为空，则容器Label中包含LabelKey=LabelValue的容器才匹配。</p><p>LabelValue默认为字符串匹配，即只有LabelValue和容器Label的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配。例如设置LabelKey为`io.kubernetes.container.name`，设置LabelValue为`^(nginx\|cube)$`，表示可匹配名为nginx、cube的容器。</p></li></ul><p>多个白名单之间为或关系，即只要容器Label满足任一白名单即可匹配。</p>                                                                |
| ExcludeLabel      | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>容器Label黑名单，用于排除不采集的容器。默认为空，表示不排除任何容器。如果您要设置容器Label黑名单，那么LabelKey必填，LabelValue可选填。</p><ul><li>如果LabelValue为空，则容器Label中包含LabelKey的容器都将被排除。</li><li><p>如果LabelValue不为空，则容器Label中包含LabelKey=LabelValue的容器才会被排除。</p><p>LabelValue默认为字符串匹配，即只有LabelValue和容器Label的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配。例如设置LabelKey为`io.kubernetes.container.name`，设置LabelValue为`^(nginx\|cube)$`，表示可匹配名为nginx、cube的容器。</p></li></ul><p>多个黑名单之间为或关系，即只要容器Label满足任一黑名单对即可被排除。</p>                                                              |
| IncludeEnv        | Map，其中EnvKey和EnvValue为String类型     | 否    | <p>设环境变量白名单，用于指定待采集的容器。默认为空，表示采集所有容器的标准输出。如果您要设置环境变量白名单，那么EnvKey必填，EnvValue可选填。</p><ul><li>如果EnvValue为空，则容器环境变量中包含EnvKey的容器都匹配。</li><li><p>如果EnvValue不为空，则容器环境变量中包含EnvKey=EnvValue的容器才匹配。</p><p>EnvValue默认为字符串匹配，即只有EnvValue和环境变量的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配，例如设置EnvKey为`NGINX_SERVICE_PORT`，设置EnvValue为`^(80|6379)$`，表示可匹配服务端口为80、6379的容器。</p></li></ul><p>多个白名单之间为或关系，即只要容器的环境变量满足任一键值对即可匹配。</p>                                                                                                             |
| ExcludeEnv        | Map，其中EnvKey和EnvValue为String类型     | 否    | <p>环境变量黑名单，用于排除不采集的容器。默认为空，表示不排除任何容器。如果您要设置环境变量黑名单，那么EnvKey必填，EnvValue可选填。</p><ul><li>如果EnvValue为空，则容器环境变量中包含EnvKey的容器的日志都将被排除。</li><li><p>如果EnvValue不为空，则容器环境变量中包含EnvKey=EnvValue的容器才会被排除。</p><p>EnvValue默认为字符串匹配，即只有EnvValue和环境变量的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配，例如设置EnvKey为`NGINX_SERVICE_PORT`，设置EnvValue为`^(80\|6379)$`，表示可匹配服务端口为80、6379的容器。</p></li></ul><p>多个黑名单之间为或关系，即只要容器的环境变量满足任一键值对即可被排除。</p>                                                                                                          |
| IncludeK8sLabel   | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>通过Kubernetes Label（定义在template.metadata中）白名单指定待采集的容器。如果您要设置Kubernetes Label白名单，那么LabelKey必填，LabelValue可选填。</p><ul><li>如果LabelValue为空，则Kubernetes Label中包含LabelKey的容器都匹配。</li><li><p>如果LabelValue不为空，则Kubernetes Label中包含LabelKey=LabelValue的容器才匹配。</p><p>LabelValue默认为字符串匹配，即只有LabelValue和Kubernetes Label的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配。例如设置LabelKey为`app`，设置LabelValue为`^(test1\|test2)$`，表示匹配Kubernetes Label中包含app:test1、app:test2的容器。</p></li></ul><p>多个白名单之间为或关系，即只要Kubernetes Label满足任一白名单即可匹配。</p>      |
| ExcludeK8sLabel   | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>通过Kubernetes Label（定义在template.metadata中）黑名单排除不采集的容器。如果您要设置Kubernetes Label黑名单，那么LabelKey必填，LabelValue可选填。</p><ul><li>如果LabelValue为空，则Kubernetes Label中包含LabelKey的容器都被排除。</li><li><p>如果LabelValue不为空，则Kubernetes Label中包含LabelKey=LabelValue的容器才会被排除。</p><p>LabelValue默认为字符串匹配，即只有LabelValue和Kubernetes Label的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配。例如设置LabelKey为`app`，设置LabelValue为`^(test1\|test2)$`，表示匹配Kubernetes Label中包含app:test1、app:test2的容器。</p></li></ul><p>多个黑名单之间为或关系，即只要Kubernetes Label满足任一黑名单对即可被排除。</p> |
| K8sNamespaceRegex | String                             | 否    | 通过Namespace名称指定采集的容器，支持正则匹配。例如设置为`^(default\|nginx)$`，表示匹配nginx命名空间、default命名空间下的所有容器。                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| K8sPodRegex       | String                             | 否    | 通过Pod名称指定待采集的容器，支持正则匹配。例如设置为`^(nginx-log-demo.*)$`，表示匹配以nginx-log-demo开头的Pod下的所有容器。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| K8sContainerRegex | String                             | 否    | 通过容器名称指定待采集的容器（Kubernetes容器名称是定义在spec.containers中），支持正则匹配。例如设置为`^(container-test)$`，表示匹配所有名为container-test的容器。                                                                                                                                                                                                                                                                                                                                                                                                                                                      |

### 数据处理参数

| 参数                   | 类型                                 | 是否必选 | 说明                                                                                                                                                                                                                                                            |
| -------------------- |------------------------------------| ---- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| BeginLineRegex       | String                             | 否    | <p>行首匹配的正则表达式。</p><p>该配置项为空，表示单行模式。</p><p>如果该表达式匹配某行的开头，则将该行作为一条新的日志，否则将此行拼接到上一条日志。</p>                                                                                                                                                                       |
| BeginLineCheckLength | Integer                            | 否    | <p>行首匹配的长度，单位：字节。</p><p>默认取值为10×1024字节。</p><p>如果行首匹配的正则表达式在前N个字节即可体现，推荐设置此参数，提升行首匹配效率。</p>                                                                                                                                                                    |
| BeginLineTimeoutMs   | Integer                            | 否    | <p>行首匹配的超时时间，单位：毫秒。</p><p>默认取值为3000毫秒。</p><p>如果3000毫秒内没有出现新日志，则结束匹配，将最后一条日志上传到日志服务。</p>                                                                                                                                                                       |
| MaxLogSize           | Integer                            | 否    | <p>日志最大长度<strong>，</strong>默认取值为0，单位：字节。</p><p>默认取值为512×1024字节。</p><p>如果日志长度超过该值，则不再继续查找行首，直接上传。</p>                                                                                                                                                          |
| ExternalK8sLabelTag  | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>设置Kubernetes Label（定义在template.metadata中）日志标签后，iLogtail将在日志中新增Kubernetes Label相关字段。</p><p>例如设置LabelKey为app，LabelValue为`k8s_label_app`，当Pod中包含Label `app=serviceA`时，会将该信息iLogtail添加到日志中，即添加字段k8s_label_app: serviceA；若不包含名为app的label时，添加空字段k8s_label_app: 。</p> |
| ExternalEnvTag       | Map，其中EnvKey和EnvValue为String类型     | 否    | <p>设置容器环境变量日志标签后，iLogtail将在日志中新增容器环境变量相关字段。</p><p>例如设置EnvKey为`VERSION`，EnvValue为`env_version`，当容器中包含环境变量`VERSION=v1.0.0`时，会将该信息以tag形式添加到日志中，即添加字段env_version: v1.0.0；若不包含名为VERSION的环境变量时，添加空字段env_version: 。</p>                                        |

### 数据处理环境变量

| 环境变量                   | 类型     | 是否必选 | 说明                                                                                                                                                                                                                       |
| ---------------------- | ------ | ---- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| ALIYUN\_LOG\_ENV\_TAGS | String | 否    | <p>设置全局环境变量日志标签后，iLogtail将在日志中新增iLogtail所在容器环境变量相关字段。多个环境变量名以`\|`分隔。</p><p>例如设置为node_name\|node_ip，当iLogtail容器中暴露相关环境变量时，会将该信息以tag形式添加到日志中，即添加字段node_ip:172.16.0.1和node_name:worknode。</p> |

## 默认日志字段

所有使用本插件上报的日志均额外携带下列字段。目前暂不支持更改。

| 字段名称                | 说明                                         |
| ------------------- | ------------------------------------------ |
| \_time\_            | 日志采集时间，例如`2021-02-02T02:18:41.979147844Z`。 |
| \_source\_          | 日志源类型，stdout或stderr。                       |
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
      - Type: service_docker_stdout
        Stdout: true
        Stderr: false
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
      - Type: service_docker_stdout
        Stdout: true
        Stderr: false
        IncludeLabel:
          io.kubernetes.container.name: "nginx"
        IncludeLabel:
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
      - Type: service_docker_stdout
        Stdout: true
        Stderr: false
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
      - Type: service_docker_stdout
        Stdout: true
        Stderr: false
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
      - Type: service_docker_stdout
        Stdout: false
        Stderr: true
        BeginLineCheckLength: 10
        BeginLineRegex: "\\d+-\\d+-\\d+.*"
```
