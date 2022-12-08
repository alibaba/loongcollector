# 使用DaemonSet模式采集K8s容器日志

`iLogtail`是阿里云日志服务（SLS）团队自研的可观测数据采集`Agent`，拥有的轻量级、高性能、自动化配置等诸多生产级别特性，可以署于物理机、虚拟机、`Kubernetes`等多种环境中来采集遥测数据。iLogtail在阿里云上服务了数万家客户主机和容器的可观测性采集工作，在阿里巴巴集团的核心产品线，如淘宝、天猫、支付宝、菜鸟、高德地图等也是默认的日志、监控、Trace等多种可观测数据的采集工具。目前iLogtail已有千万级的安装量，每天采集数十PB的可观测数据，广泛应用于线上监控、问题分析/定位、运营分析、安全分析等多种场景，在实战中验证了其强大的性能和稳定性。

在当今云原生的时代，我们坚信开源才是iLogtail最优的发展策略，也是释放其最大价值的方法。因此，我们决定将`iLogtail`开源，期望同众多开发者一起将iLogtail打造成世界一流的可观测数据采集器。

## K8s的日志架构 <a href="#x6yil" id="x6yil"></a>

日志对于构建数据驱动的应用架构至关重要。在Kubernetes分布式的容器环境中，各个业务容器的日志四处散落，用户往往希望拥有一个中心化的日志管理方案，以使不同应用、格式各异的相关日志能够一起进行处理分析。K8s已经为此提供了必备的基础资源和设施。本文将简要介绍K8s的日志架构并演示如何通过iLogtail统一采集日志。

![数据驱动的应用架构](<../.gitbook/assets/getting-started/k8s-daemonset-to-kafka/k8s-data-driven.png>)

K8s官方推荐的日志架构为将应用的日志输出到标准输出流(stdout)或标准错误流(stderr)，然后由Docker或Containerd+Kubelet对日志输出进行重定向存储管理。Kubernetes提供了kubectl logs命令供用户查询日志，该命令同时可接受-p/--previous参数查询最近一个退出的容器实例日志，该参数在排查崩溃或重启的容器时尤其有用。

然而，如果pod从节点上被删除或节点崩溃，那么其下所有容器的日志将一并丢失，用户将无法再查询这些日志。为了避免这种情况，用户应该使用与业务容器和节点生命周期独立的日志采集和存储系统。Kubenertes没有原生提供这样的解决方案，但通过Kubernetes API和controllers用户可以使用偏好的日志组件自行搭建。

## K8s采集日志的几种方式 <a href="#xaamx" id="xaamx"></a>

大体上，在当前的K8s架构中采集日志通常有以下几种常见方式：

1. 在每个节点上部署日志采集Agent
2. 使用sidecar模式在业务Pod内部署日志采集容器
3. 在业务应用内直接向服务端发送日志

这里我们仅讨论第一种方式。

### 在每个节点上部署日志采集Agent <a href="#cgooc" id="cgooc"></a>

在这种方式下，日志采集Agent通常以一个能访问节点上所有日志的容器存在。通常生产集群有很多节点，每个节点都需要部署一个采集Agent。面对这种情况，最简单的部署方式是直接只用K8s提供的Deployment进行容器编排。DaemonSet controller会定期检查节点的变化情况，并自动保证每个节点上有且只有一个采集Agent容器。

![图片源：https://kubernetes.io/docs/concepts/cluster-administration/logging/](../.gitbook/assets/getting-started/k8s-daemonset-to-kafka/daemonset-logging-arch.png)

使用DaemonSet方式采集K8s日志有以下优点，通常是首选被广泛使用：

1. 占用节点资源较少，也不随业务容器数量增加而变多
2. 对业务应用无侵入，新接入应用无需改造适配
3. 单个节点上的日志聚合发送，对接收端更加友好

然而这种方式也存在一些限制：

1. 无法支持采集业务容器中挂载的所有类型PVC目录，如挂载了云盘
2. 无法支持采集所有类型的容器运行时，如[Kata](https://katacontainers.io/)
3. 无法支持超出单Agent采集能力的日志流量，如1GB/s

若遇到上述情况则应考虑其他采集方式。

## 理解iLogtail采集容器日志原理 <a href="#xnawr" id="xnawr"></a>

iLogtail支持全场景的容器数据采集，包括Docker和K8s环境。iLogtail通过[docker\_center插件](https://github.com/alibaba/ilogtail/blob/main/helper/docker\_center.go)与节点上的容器运行时进行通信，发现节点的容器列表并维护容器和日志采集路径映射。然后，对于容器标准输出，iLogtail使用[input\_docker\_stdout插件](https://github.com/alibaba/ilogtail/blob/main/plugins/input/docker/stdout/input\_docker\_stdout.go)对日志进行采集，包括容器筛选和多行切分等步骤；对于容器文件则使用[input\_docker\_event插件](https://github.com/alibaba/ilogtail/blob/main/plugins/input/docker/event/input\_docker\_event.go)结合C++内核实现，前者负责容器筛选，后者提供高效的文件发现、采集能力。iLogtail支持DaemonSet、Sidecar、CRD等多种部署方式，为应对不同使用场景提供了灵活的部署能力。而iLogtail采用全局容器列表和通过Kubernetes CRI协议获取容器信息的设计，使其在权限和组件依赖上相比其他开源更加轻量级，并且拥有更高的采集效率。

![iLogtail采集K8s容器日志](<../.gitbook/assets/getting-started/k8s-daemonset-to-kafka/ilogtail-k8s-daemonset.png>)

iLogtail支持使用容器标签、环境变量、K8s标签、Pod名称、命名空间等多种方式进行容器筛选，为容器日志采集提供了极强的灵活性。

**容器筛选**

黑名单或白名单

* 容器Label
* K8s Label
* 环境变量

正则匹配

* K8s Namespace名称
* K8s Pod名称
* 容器名称

**数据处理**

* 支持采集多行日志（例如Java Stack日志等）。
* 支持自动关联Kubernetes Label信息。
* 支持自动关联容器Meta信息（例如容器名、IP、镜像、Pod、Namespace、环境变量等）。
* 支持自动关联宿主机Meta信息（例如宿主机名、IP、环境变量等）。

## 部署iLogtail采集业务日志到Kafka <a href="#tsshm" id="tsshm"></a>

这部分将完成数据驱动应用架构的第一步，将日志统一采集写入Kafka。本章节所使用的配置可在[GitHub](https://github.com/alibaba/ilogtail/blob/main/k8s_templates/ilogtail-daemonset-kafka.yaml)下载，容器标准输出插件详细配置可移步[iLogtail用户手册](https://ilogtail.gitbook.io/ilogtail-docs/data-pipeline/input/input-docker-stdout)。

#### 前提条件 <a href="#sra69" id="sra69"></a>

1. K8s集群的搭建和具备访问K8s集群的[kubectl](https://kubernetes.io/docs/tasks/tools/install-kubectl-linux/)
2. Kafka的搭建和具备访问[Kafka](https://kafka.apache.org/quickstart)的consumer client
3. 已经创建了名为access-log和error-log的[topic](https://ilogtail.gitbook.io/ilogtail-docs/getting-started/how-to-collect-to-kafka#hvouy)

#### 第一步，创建命名空间和配置文件 <a href="#is9ro" id="is9ro"></a>

推荐将iLogtail部署在独立的命名空间站以便管理。

ilogtail-ns.yaml:

```yaml  {.line-numbers}
apiVersion: v1
kind: Namespace
metadata:
  name: ilogtail
```

```bash
kubectl apply -f ilogtail-ns.yaml
```

当前iLogtail社区版暂时不支持配置热加载，因此这里我们先创建配置，后启动iLogtail容器。若后续需要更改，可以修改configmap后，重启ilogtail的pod/container使其生效。

ilogtail-user-configmap.yaml:

```yaml  {.line-numbers}
apiVersion: v1
kind: ConfigMap
metadata:
  name: ilogtail-user-cm
  namespace: ilogtail
data:
  nginx_stdout.yaml: |
    enable: true
    inputs:
      - Type: service_docker_stdout
        Stderr: false
        Stdout: true
        IncludeK8sLabel:
          app: nginx
    flushers:
      - Type: flusher_kafka_v2
        Brokers:
          - <kafka_host>:<kafka_port>
        Topic: access-log
  nginx_stderr.yaml: |
    enable: true
    inputs:
      - Type: service_docker_stdout
        Stderr: true
        Stdout: false
        K8sNamespaceRegex: "^(default)$"
        K8sPodRegex: "^(nginx-.*)$"
        K8sContainerRegex: "nginx"
    flushers:
      - Type: flusher_kafka_v2
        Brokers:
          - <kafka_host>:<kafka_port>
        Topic: error-log
```

```bash
kubectl apply -f ilogtail-user-configmap.yaml
```

这里的ConfigMap期望以文件夹的方式挂载到iLogtail容器中作为采集配置目录，因此可以包含多个iLogtail采集配置文件，第7行起到最后19行为一个采集配置，将nginx的标准输出采集到Kafka access-log主题，10-33为另一个采集配置，将nginx的标准错误输出到Kafka error-log主题。

第13-14和26-28行展示了如何为日志采集筛选容器，前者使用Kubernetes Label作为筛选条件，后者则使用了Namespace、Pod和Container名称作筛选，所有支持的配置项可以参考iLogtail用户手册中的[容器标准输出](https://ilogtail.gitbook.io/ilogtail-docs/data-pipeline/input/input-docker-stdout)。

#### 第二步，部署iLogtail DaemonSet <a href="#mwxf7" id="mwxf7"></a>

ilogtail-daemonset.yaml:

```yaml  {.line-numbers}
apiVersion: apps/v1
kind: DaemonSet
metadata:
  name: ilogtail-ds
  namespace: ilogtail
  labels:
    k8s-app: logtail-ds
spec:
  selector:
    matchLabels:
      k8s-app: logtail-ds
  template:
    metadata:
      labels:
        k8s-app: logtail-ds
    spec:
      tolerations:
      - key: node-role.kubernetes.io/master
        effect: NoSchedule
      containers:
      - name: logtail
        env:
          - name: cpu_usage_limit
            value: "1"
          - name: mem_usage_limit
            value: "512"
        image: >-
          sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:latest
        imagePullPolicy: IfNotPresent
        resources:
          limits:
            cpu: 1000m
            memory: 1Gi
          requests:
            cpu: 400m
            memory: 384Mi
        volumeMounts:
          - mountPath: /var/run
            name: run
          - mountPath: /logtail_host
            mountPropagation: HostToContainer
            name: root
            readOnly: true
          - mountPath: /usr/local/ilogtail/checkpoint
            name: checkpoint
          - mountPath: /usr/local/ilogtail/user_yaml_config.d
            name: user-config
            readOnly: true
      dnsPolicy: ClusterFirstWithHostNet
      hostNetwork: true
      volumes:
        - hostPath:
            path: /var/run
            type: Directory
          name: run
        - hostPath:
            path: /
            type: Directory
          name: root
        - hostPath:
            path: /lib/var/ilogtail-ilogtail-ds/checkpoint
            type: DirectoryOrCreate
          name: checkpoint
        - configMap:
            defaultMode: 420
            name: ilogtail-user-cm
          name: user-config
```

```bash
kubectl apply -f ilogtail-deployment.yaml
```

配置文件的17-19行定义了部署节点的容忍性：不在master节点部署。

23-26通过容器环境变量对iLogtail进行了系统配置，这里配置了cpu和memory上限。完整的系统配置说明可以参考iLogtail用户手册中的[系统参数](https://ilogtail.gitbook.io/ilogtail-docs/configuration/system-config)。

31-36行定义了采集Agent容器允许使用的资源范围。若需要采集的日志文件数量很多，则需要适当地放宽资源限制。

配置文件的38-48行挂载了一些目录，说明如下：

`/var/run`：iLogtail与容器运行时通信的socket

`/logtail_host`：iLogtail通过挂载主机目录获取节点上所有容器的日志

`/usr/local/ilogtail/checkpoint`：将状态持久化到主机磁盘，iLogtail容器重启不丢失

`/usr/local/ilogtail/user_yaml_config.d`：将configmap中的配置挂载到容器中

#### 第三步，部署Nginx，发送测试请求并验证 <a href="#eiejy" id="eiejy"></a>

nginx-deployment.yaml:

```yaml  {.line-numbers}
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nginx
  namespace: default
  labels:
    app: nginx
spec:
  replicas: 1
  selector:
    matchLabels:
      app: nginx
  template:
    metadata:
      labels:
        app: nginx
    spec:
      containers:
        - image: 'nginx:latest'
          name: nginx
          ports:
            - containerPort: 80
              name: http
              protocol: TCP
          resources:
            requests:
              cpu: 100m
              memory: 100Mi
```

```bash
kubectl apply -f nginx-mock-deployment.yaml
```

启动Kafka消费端开始观察日志：

```bash
# In Terminal 1
bin/kafka-console-consumer.sh --topic access-log --bootstrap-server <kafka_host>:<kafka_port>
# In Terminal 2
bin/kafka-console-consumer.sh --topic error-log --bootstrap-server <kafka_host>:<kafka_port>
```

给nginx发送几条测试请求，如：

```
kubectl exec nginx-76d49876c7-r892w -- curl localhost/hello/ilogtail
```

查看Kafka消费端应该已经有日志输出了。从日志中同时可以看到，iLogtail默认对采集的日志进行了必要的标注如\_source\_标注了日志是标准输出还是标准错误流的，_container\_name_、_container\_name_、\_container\_ip\_标注了日志来源的容器。

```json
# In Terminal 1
{"Time":1657727155,"Contents":[{"Key":"content","Value":"::1 - - [13/Jul/2022:15:45:54 +0000] \"GET /hello/ilogtail HTTP/1.1\" 404 153 \"-\" \"curl/7.74.0\" \"-\""},{"Key":"_time_","Value":"2022-07-13T23:45:54.976593653+08:00"},{"Key":"_source_","Value":"stdout"},{"Key":"_image_name_","Value":"docker.io/library/nginx:latest"},{"Key":"_container_name_","Value":"nginx"},{"Key":"_pod_name_","Value":"nginx-76d49876c7-r892w"},{"Key":"_namespace_","Value":"default"},{"Key":"_pod_uid_","Value":"07f75a79-da69-40ac-ae2b-77a632929cc6"},{"Key":"_container_ip_","Value":"10.223.0.154"}]}
# In Terminal 2
{"Time":1657727190,"Contents":[{"Key":"content","Value":"2022/07/13 15:46:29 [error] 32#32: *6 open() \"/usr/share/nginx/html/hello/ilogtail\" failed (2: No such file or directory), client: ::1, server: localhost, request: \"GET /hello/ilogtail HTTP/1.1\", host: \"localhost\""},{"Key":"_time_","Value":"2022-07-13T23:45:54.976593653+08:00"},{"Key":"_source_","Value":"stderr"},{"Key":"_image_name_","Value":"docker.io/library/nginx:latest"},{"Key":"_container_name_","Value":"nginx"},{"Key":"_pod_name_","Value":"nginx-76d49876c7-r892w"},{"Key":"_namespace_","Value":"default"},{"Key":"_pod_uid_","Value":"07f75a79-da69-40ac-ae2b-77a632929cc6"},{"Key":"_container_ip_","Value":"10.223.0.154"}]}
```

#### 第四步，配置正则解析，结构化日志 <a href="#s3zn3" id="s3zn3"></a>

未经处理的原始日志使用不便、可读性较差，可以利用iLogtail内置的端上处理能力使日志结构化。

替换ilogtail-user-configmap.yaml的1-19行，保存为ilogtail-user-configmap-processor.yaml。

```yaml  {.line-numbers}
  nginx_stdout.yaml: |
    enable: true
    inputs:
      - Type: service_docker_stdout
        Stderr: false
        Stdout: true
        IncludeK8sLabel:
          app: nginx
    processors:
      - Type: processor_regex
        SourceKey: content
        Regex: '([\d\.:]+) - (\S+) \[(\S+) \S+\] \"(\S+) (\S+) ([^\\"]+)\" (\d+) (\d+) \"([^\\"]*)\" \"([^\\"]*)\" \"([^\\"]*)\"'
        Keys:
          - remote_addr
          - remote_user
          - time_local
          - method
          - path
          - protocol
          - status
          - body_bytes_sent
          - http_referer
          - http_user_agent
          - http_x_forwarded_for
    flushers:
      - Type: flusher_kafka_v2
        Brokers:
          - <kafka_host>:<kafka_port>
        Topic: access-log
```

```bash
kubectl apply -f ilogtail-user-configmap-processor.yaml
```

重启iLogtail容器使其生效。

```
kubectl exec -n ilogtail ilogtail-ds-krm8t -- /bin/sh -c "kill 1"
```

再次发送测试请求，观察Kafka消费端access-log主题输出。稍加格式化，可以看到每一条记录都进行了字段提取，成为了易读易用的结构化的日志。

```json
{"Time":1657729579,"Contents":[
  {"Key":"_time_","Value":"2022-07-14T00:26:19.304905535+08:00"},
  {"Key":"_source_","Value":"stdout"},
  {"Key":"_image_name_","Value":"docker.io/library/nginx:latest"},
  {"Key":"_container_name_","Value":"nginx"},
  {"Key":"_pod_name_","Value":"nginx-76d49876c7-r892w"},
  {"Key":"_namespace_","Value":"default"},
  {"Key":"_pod_uid_","Value":"07f75a79-da69-40ac-ae2b-77a632929cc6"},
  {"Key":"_container_ip_","Value":"10.223.0.154"},
  {"Key":"remote_addr","Value":"::1"},
  {"Key":"remote_user","Value":"-"},
  {"Key":"time_local","Value":"13/Jul/2022:16:26:19"},
  {"Key":"method","Value":"GET"},
  {"Key":"url","Value":"/hello/ilogtail"},
  {"Key":"protocol","Value":"HTTP/1.1"},
  {"Key":"status","Value":"404"},
  {"Key":"body_bytes_sent","Value":"153"},
  {"Key":"http_referer","Value":"-"},
  {"Key":"http_user_agent","Value":"curl/7.74.0"},
  {"Key":"http_x_forwarded_for","Value":"-"}]}
```

## 采集容器内的文件
某些应用选择将日志打印在容器内使用自带的日志机制进行轮转，iLogtail也支持这种场景的日志采集。这里我们以采集json格式日志为例。

前提条件和对iLogtail DaemonSet的部署不再赘述，仅关注配置和验证过程。

#### 第一步，配置容器日志采集 <a href="#fcfc" id="fcfc"></a>

ilogtail-user-configmap.yaml:

```yaml {.line-numbers}
apiVersion: v1
kind: ConfigMap
metadata:
  name: ilogtail-user-cm
  namespace: ilogtail
data:
  json_log.yaml: |
    enable: true
    inputs:
      - Type: file_log
        LogPath: /root/log
        FilePattern: "json.log"
        DockerFile: true
        DockerIncludeLabel:
          io.kubernetes.container.name: json-log
    processors:
      - Type: processor_json
        SourceKey: content
        KeepSource: false
        ExpandDepth: 1
        ExpandConnector: ""
    flushers:
      - Type: flusher_kafka_v2
        Brokers:
          - 39.99.61.125:9092
        Topic: json-log
```


第13行表明采集的文件来自容器内，14-15行使用容器名对目标容器进行筛选。17-21行使用了json处理插件对日志进行结构化解析。

```bash
kubectl apply -f ilogtail-user-configmap.yaml
```

重启iLogtail容器使其生效。

```
kubectl exec -n ilogtail ilogtail-ds-krm8t -- /bin/sh -c "kill 1"
```

#### 第二步，部署测试容器，生成日志并验证 <a href="#rzyz" id="rzyz"></a>

json-log-deployment.yaml:

```yaml {.line-numbers}
apiVersion: apps/v1
kind: Deployment
metadata:
  labels:
    app: json-log
  name: json-log
  namespace: default
spec:
  replicas: 1
  selector:
    matchLabels:
      app: json-log
  template:
    metadata:
      labels:
        app: json-log
    spec:
      containers:
        - args:
            - >-
              mkdir -p /root/log; while true; do date +'{"time":"+%Y-%m-%d
              %H:%M:%S","message":"Hello, iLogtail!"}' >>/root/log/json.log;
              sleep 10; done
          command:
            - /bin/sh
            - '-c'
            - '--'
          image: 'alpine:3.9.6'
          name: json-log
          volumeMounts:
            - mountPath: /etc/localtime
              name: volume-localtime
      volumes:
        - hostPath:
            path: /etc/localtime
            type: ''
          name: volume-localtime
```

```bash
kubectl apply -f json-log-deployment.yaml
```

启动Kafka消费端开始观察日志：

```bash
bin/kafka-console-consumer.sh --topic json-log --bootstrap-server <kafka_host>:<kafka_port>
```

可以看到消费端已经有日志输出，并且进行了结构化解析：
```json
{"Time":1658341942,"Contents":[
  {"Key":"__tag__:__path__","Value":"/root/log/json.log"},
  {"Key":"__tag__:__user_defined_id__","Value":"default"},
  {"Key":"__tag__:_container_ip_","Value":"10.223.0.189"},
  {"Key":"__tag__:_image_name_","Value":"docker.io/library/alpine:3.9.6"},{"Key":"__tag__:_container_name_","Value":"json-log"},
  {"Key":"__tag__:_pod_name_","Value":"json-log-5df95f9f84-dhj2l"},
  {"Key":"__tag__:_namespace_","Value":"default"},
  {"Key":"__tag__:_pod_uid_","Value":"e42818ef-75c4-4854-9fe0-4dd7c7f7ccd1"},
  {"Key":"time","Value":"+2022-07-21 02:32:22"},
  {"Key":"message","Value":"Hello, iLogtail!"}]}
```

## 总结 <a href="#qtdmu" id="qtdmu"></a>

以上，我们演示了如何利用K8s提供的基础能力来快速搭建一套集群集采集日志的基础设施。我们利用了K8s的DaemonSet自动在每一个节点上部署iLogtail，使用了ConfigMap进行配置分发。强大的容器筛选能力和元信息处理能力使iLogtail成为采集K8s容器日志的最佳选择之一。未来我们将进一步开源iLogtail的K8s Operator，以CRD的形式管理配置，进一步强化K8s环境下对iLogtail的管控能力。

一套由数据驱动的应用架构，从数据采集到数据应用，数据采集只是开始，如果对数据的传输、存储、处理和查询有更高的要求也可以基于SLS构建高可用免运维的数据平台。

## **关于iLogtail**

* GitHub：[https://github.com/alibaba/ilogtail](https://github.com/alibaba/ilogtail)

* 社区版文档：[https://ilogtail.gitbook.io/ilogtail-docs/about/readme](https://ilogtail.gitbook.io/ilogtail-docs/about/readme)

* 交流群请扫描

![](../.gitbook/assets/chatgroup.png)



