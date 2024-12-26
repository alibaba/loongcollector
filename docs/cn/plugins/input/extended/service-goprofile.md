# GO Profile

## 简介

`service_go_profile` `input`插件可以采集Golang pprof 性能数据。

## 配置参数

### 全局参数

| 参数              | 类型，默认值                                                    | 说明                                  |
|-----------------|-----------------------------------------------------------|-------------------------------------|
| Mode            | string，无默认值（必填）                                           | 工作类型，目前支持`host`与`kubernets` 2种环境工作。 |
| Config          | object，无默认值（必填）                                           | 详见不同工作模式子配置。                        |
| Interval        | int，`10`                                                  | 抓取性能数据间隔，单位秒                        |
| Timeout         | int，`15`                                                  | 抓取性能数据超时时间，单位秒                      |
| BodyLimitSize   | int，`10240`                                               | 抓取性能数据Body最大上限，单位KB                 |
| EnabledProfiles | []string，`["cpu", "mem", "goroutines", "mutex", "block"]` | 抓取性能数据类型。                           |
| Labels          | map[string]string，`{}`                                    | 性能数据全局追加标签。                         |

### host模式子配置

*对于静态host模式，需要指定service标签标明归属服务。*

1. 子配置

| 参数        | 类型，默认值             | 说明                |
|-----------|--------------------|-------------------|
| Addresses | []Address，无默认值（必填） | Address Object 数组 |

2. Address 配置

| 参数             | 类型，默认值                 | 说明           |
|----------------|------------------------|--------------|
| Host           | string，无默认值（必填）        | 实例地址         |
| Port           | int，无默认值（必填）           | 实例端口         |
| InstanceLabels | map[string]string，`{}` | 具体实例性能数据追加标签 |

### kubernetes模式子配置

*待采集容器需要先声明ILOGTAIL_PROFILE_PORT={PORT} 环境变量，其后通过下述配置二次过滤。*

| 参数                    | 类型                                 | 是否必选 | 说明                                                                                                                                                                                                                                                                                                                                                                                             |
|-----------------------|------------------------------------|------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| IncludeContainerLabel | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>容器Label白名单，用于指定待采集的容器。默认为空，表示采集所有容器。如果您要设置容器Label白名单，那么LabelKey必填，LabelValue可选填。</p><ul><li>如果LabelValue为空，则容器Label中包含LabelKey的容器都匹配。</li><li><p>如果LabelValue不为空，则容器Label中包含LabelKey=LabelValue的容器才匹配。</p><p>LabelValue默认为字符串匹配，即只有LabelValue和容器Label的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配。例如设置LabelKey为`io.kubernetes.container.name`，设置LabelValue为`^(nginx\                                |cube)$`，表示可匹配名为nginx、cube的容器。</p></li></ul><p>多个白名单之间为或关系，即只要容器Label满足任一白名单即可匹配。</p>                                                                |
| ExcludeContainerLabel | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>容器Label黑名单，用于排除不采集的容器。默认为空，表示不排除任何容器。如果您要设置容器Label黑名单，那么LabelKey必填，LabelValue可选填。</p><ul><li>如果LabelValue为空，则容器Label中包含LabelKey的容器都将被排除。</li><li><p>如果LabelValue不为空，则容器Label中包含LabelKey=LabelValue的容器才会被排除。</p><p>LabelValue默认为字符串匹配，即只有LabelValue和容器Label的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配。例如设置LabelKey为`io.kubernetes.container.name`，设置LabelValue为`^(nginx\                           |cube)$`，表示可匹配名为nginx、cube的容器。</p></li></ul><p>多个黑名单之间为或关系，即只要容器Label满足任一黑名单对即可被排除。</p>                                                              |
| IncludeEnv            | Map，其中EnvKey和EnvValue为String类型     | 否    | <p>设环境变量白名单，用于指定待采集的容器。默认为空，表示采集所有容器。如果您要设置环境变量白名单，那么EnvKey必填，EnvValue可选填。</p><ul><li>如果EnvValue为空，则容器环境变量中包含EnvKey的容器都匹配。</li><li><p>如果EnvValue不为空，则容器环境变量中包含EnvKey=EnvValue的容器才匹配。</p><p>EnvValue默认为字符串匹配，即只有EnvValue和环境变量的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配，例如设置EnvKey为`NGINX_SERVICE_PORT`，设置EnvValue为`^(80                                                                              |6379)$`，表示可匹配服务端口为80、6379的容器。</p></li></ul><p>多个白名单之间为或关系，即只要容器的环境变量满足任一键值对即可匹配。</p>                                                                                                             |
| ExcludeEnv            | Map，其中EnvKey和EnvValue为String类型     | 否    | <p>环境变量黑名单，用于排除不采集的容器。默认为空，表示不排除任何容器。如果您要设置环境变量黑名单，那么EnvKey必填，EnvValue可选填。</p><ul><li>如果EnvValue为空，则容器环境变量中包含EnvKey的容器的日志都将被排除。</li><li><p>如果EnvValue不为空，则容器环境变量中包含EnvKey=EnvValue的容器才会被排除。</p><p>EnvValue默认为字符串匹配，即只有EnvValue和环境变量的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配，例如设置EnvKey为`NGINX_SERVICE_PORT`，设置EnvValue为`^(80\                                                                      |6379)$`，表示可匹配服务端口为80、6379的容器。</p></li></ul><p>多个黑名单之间为或关系，即只要容器的环境变量满足任一键值对即可被排除。</p>                                                                                                          |
| IncludeK8sLabel       | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>通过Kubernetes Label（定义在template.metadata中）白名单指定待采集的容器。如果您要设置Kubernetes Label白名单，那么LabelKey必填，LabelValue可选填。</p><ul><li>如果LabelValue为空，则Kubernetes Label中包含LabelKey的容器都匹配。</li><li><p>如果LabelValue不为空，则Kubernetes Label中包含LabelKey=LabelValue的容器才匹配。</p><p>LabelValue默认为字符串匹配，即只有LabelValue和Kubernetes Label的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配。例如设置LabelKey为`app`，设置LabelValue为`^(test1\    |test2)$`，表示匹配Kubernetes Label中包含app:test1、app:test2的容器。</p></li></ul><p>多个白名单之间为或关系，即只要Kubernetes Label满足任一白名单即可匹配。</p>      |
| ExcludeK8sLabel       | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>通过Kubernetes Label（定义在template.metadata中）黑名单排除不采集的容器。如果您要设置Kubernetes Label黑名单，那么LabelKey必填，LabelValue可选填。</p><ul><li>如果LabelValue为空，则Kubernetes Label中包含LabelKey的容器都被排除。</li><li><p>如果LabelValue不为空，则Kubernetes Label中包含LabelKey=LabelValue的容器才会被排除。</p><p>LabelValue默认为字符串匹配，即只有LabelValue和Kubernetes Label的值完全相同才会匹配。如果该值以`^`开头并且以`$`结尾，则为正则匹配。例如设置LabelKey为`app`，设置LabelValue为`^(test1\ |test2)$`，表示匹配Kubernetes Label中包含app:test1、app:test2的容器。</p></li></ul><p>多个黑名单之间为或关系，即只要Kubernetes Label满足任一黑名单对即可被排除。</p> |
| K8sNamespaceRegex     | String                             | 否    | 通过Namespace名称指定采集的容器，支持正则匹配。例如设置为`^(default\                                                                                                                                                                                                                                                                                                                                                   |nginx)$`，表示匹配nginx命名空间、default命名空间下的所有容器。                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| K8sPodRegex           | String                             | 否    | 通过Pod名称指定待采集的容器，支持正则匹配。例如设置为`^(nginx-log-demo.*)$`，表示匹配以nginx-log-demo开头的Pod下的所有容器。                                                                                                                                                                                                                                                                                                            |
| K8sContainerRegex     | String                             | 否    | 通过容器名称指定待采集的容器（Kubernetes容器名称是定义在spec.containers中），支持正则匹配。例如设置为`^(container-test)$`，表示匹配所有名为container-test的容器。                                                                                                                                                                                                                                                                                 |
| ExternalK8sLabelTag   | Map，其中LabelKey和LabelValue为String类型 | 否    | <p>设置Kubernetes Label（定义在template.metadata中）标签后，iLogtail将在性能数据中新增Kubernetes Label相关字段。</p><p>例如设置LabelKey为app，LabelValue为`k8s_label_app`，当Pod中包含Label `app=serviceA`时，会将该信息iLogtail添加到性能数据中，即添加字段k8s_label_app: serviceA；若不包含名为app的label时，添加空字段k8s_label_app: 。</p>                                                                                                                            |
| ExternalEnvTag        | Map，其中EnvKey和EnvValue为String类型     | 否    | <p>设置容器环境变量性能数据标签后，iLogtail将在性能数据中新增容器环境变量相关字段。</p><p>例如设置EnvKey为`VERSION`，EnvValue为`env_version`，当容器中包含环境变量`VERSION=v1.0.0`时，会将该信息以tag形式添加到性能数据中，即添加字段env_version: v1.0.0；若不包含名为VERSION的环境变量时，添加空字段env_version: 。</p>                                                                                                                                                                         |

## 样例

### 采集host模式性能数据

1. 配置样例

```yaml
    inputs:
      - Type: service_go_profile
        Mode: host
        Labels:
          global: outer
          service: service
        Config:
          Addresses:
            - Host: "127.0.0.1"
              Port: 18689
              InstanceLabels:
                service: inner
```

2. 采集结果

```plain
2023-04-03 14:04:52 [INF] [flusher_stdout.go:120] [Flush] [1.0#PluginProject_0##Config0,PluginLogstore_0]       {"name":"github.com/denisenkom/go-mssqldb/internal/cp.init /Users/evan/go/pkg/mod/github.com/denisenkom/go-mssqldb@v0.12.2/internal/cp/cp950.go","stack":"runtime.doInit /usr/local/go/src/runtime/proc.go\nruntime.doInit /usr/local/go/src/runtime/proc.go\nruntime.doInit /usr/local/go/src/runtime/proc.go\nruntime.doInit /usr/local/go/src/runtime/proc.go\nruntime.doInit /usr/local/go/src/runtime/proc.go\nruntime.main /usr/local/go/src/runtime/proc.go","stackID":"c4bbfc9dff64151d","language":"go","type":"profile_mem","dataType":"CallStack","durationNs":"0","profileID":"f6a443c0-cca0-45ac-b50a-b1e25a48fb74","labels":"{\"__name__\":\"service\",\"global\":\"outer\",\"instance\":\"inner\",\"job\":\"1_0_pluginproject_0__config0\"}","units":"bytes","valueTypes":"inuse_space","aggTypes":"sum","val":"1615794.00","__time__":"1680501892"}:  
```

### 采集kubernets模式性能数据

```yaml
    inputs:
      - Type: service_go_profile
        Mode: kubernetes
        Config:
          IncludeK8sLabel:
            app: golang-pull
        Labels:
          global: outer

```

2. 采集结果

```text
2023-04-03 06:07:13 [INF] [flusher_stdout.go:120] [Flush] [1.0#PluginProject_0##Config0,PluginLogstore_0]       {"name":"runtime.memclrNoHeapPointers /usr/local/go/src/runtime/memclr_amd64.s","stack":"runtime.mallocgc /usr/local/go/src/runtime/malloc.go\nruntime.makeslice /usr/local/go/src/runtime/slice.go\nmain.memNormal /output/main.go\nmain.main.func1 /output/main.go","stackID":"36108813ff189cc4","language":"go","type":"profile_cpu","dataType":"CallStack","durationNs":"10000045532","profileID":"7bb6291c-b358-43fa-888a-dec7676552e4","labels":"{\"__name__\":\"golang-pull\",\"container\":\"golang-pull\",\"instance\":\"10.175.2.230:8080\",\"job\":\"1_0_pluginproject_0__config0\",\"label\":\"outer\",\"namespace\":\"profile\",\"pod\":\"golang-pull-6b946f8f6c-vhv9t\"}","units":"nanoseconds","valueTypes":"cpu","aggTypes":"sum","val":"20000000.00","__time__":"1680502023"}:
```
