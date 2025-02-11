# 容器场景iLogtail与Filebeat性能对比测试

## 前言

前段时间, iLogtail（[https://github.com/alibaba/loongcollector](https://github.com/alibaba/loongcollector)）阿里千万实例可观测采集器开源，其中介绍了iLogtail采集性能可以达到单核100MB/s，相比开源采集Agent有5-10倍性能优势。很多小伙伴好奇iLogtail具体的性能数据和资源消耗如何，本文将针对目前业界使用度较高且性能相对较优的Agent FileBeat进行对比，测试这两个Agent在不同压力场景下的表现如何。

## 测试试验描述

随着Kubernetes 普及，Kubernetes 下的日志收集的需求也日益常态化，因此下文将分别进行容器标准输出流采集与容器内静态文件采集对比试验（使用静态文件采集的小伙伴可以参考容器内静态文件采集对比试验, iLogtail 纯静态文件采集会略优于试验2容器内文件静态采集），试验项具体如下：

- **实验1**：恒定采集配置4，Filebeat & iLogtail 在原始日志产生速率 1M/s、2M/s、 3M/s 下的**标准输出流采集**性能对比。
- **实验2**：恒定采集配置4，Filebeat & iLogtail 在原始日志产生速率 1M/s、2M/s、 3M/s 下的**容器内文件采集**性能对比。

而在真实的生产环境中，日志采集组件的可运维性也至关重要，为运维与后期升级便利，相比于Sidecar模式，K8s下采用**Daemonset模式**部署采集组件更加常见。但由于**Daemonset** 同时将整个集群的采集配置下发到各个采集节点的特性，单个采集节点正在工作的配置必定小于全量采集配置数目，因此我们还会进行以下2部分试验，验证采集配置的膨胀是否会影响采集器的工作效率：
​

- **实验3**：恒定输入速率3M/s，Filebeat & iLogtail 在采集配置50、100、500、1000 份下的**标准输出流采集**性能对比。
- **实验4**：恒定输入速率3M/s，Filebeat & iLogtail 在采集配置50、100、500、1000 份下的**容器内文件采集**性能对比。

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537108680-be968e23-750e-4aae-a63d-61ec7d881dbd.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&id=N2pXy&margin=%5Bobject%20Object%5D&name=image.png&originHeight=400&originWidth=500&originalType=url&ratio=1&rotation=0&showTitle=false&size=42799&status=done&style=none&taskId=uf19fc34e-90ff-4e3b-908a-9abf6dbd4cb&title=)
最后会进行iLogtail 的大流量压测，具体如下：

- 实验5：iLogtail 在 5M/s、10M/s、10M/s、40M/s 下的**标准输出流采集**性能。
- 实验5：iLogtail 在 5M/s、10M/s、10M/s、40M/s 下的**容器内文件采集**性能。

## 试验环境

所有采集环境数据存储于[https://github.com/EvanLjp/ilogtail-comparison](https://github.com/EvanLjp/ilogtail-comparison)， 感兴趣的同学可以自己动手进行整个对比测试实验， 以下部分分别描述了不同采集模式的具体配置，如果只关心采集对比结果，可以直接跳过此部分继续阅读。

### 环境

运行环境：阿里云ACK Pro 版本
节点配置：ecs.g6.xlarge (4 vCPU 16GB) 磁盘ESSD
底层容器：Containerd
iLogtail版本：1.0.28
FileBeat版本：v7.16.2

### 数据源

对于数据源，我们首先去除因正则解析或多行拼接能力带来的差异，仅仅以最基本的单行采集进行对比，数据产生源模拟产生nginx访问日志，单条日志大小为283B，以下配置描述了1000条/s 速率下的输入源：

```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: nginx-log-demo-0
  namespace: default
spec:
  template:
    metadata:
      name: nginx-log-demo-0
    spec:
      restartPolicy: Never
      containers:
        - name: nginx-log-demo-0
          image: registry.cn-hangzhou.aliyuncs.com/log-service/docker-log-test:latest
          command: ["/bin/mock_log"]
          args:  ["--log-type=nginx",  "--path=/var/log/medlinker/access.log", "--total-count=1000000000", "--log-file-size=1000000000", "--log-file-count=2", "--logs-per-sec=1000"]
          volumeMounts:
            - name: path
              mountPath: /var/log/medlinker
              subPath: nginx-log-demo-0
          resources:
            limits:
              memory: 200Mi
            requests:
              cpu: 10m
              memory: 10Mi
      volumes:
      - name: path
        hostPath:
          path: /testlog
          type: DirectoryOrCreate
      nodeSelector:
        kubernetes.io/hostname: cn-beijing.192.168.0.140
```

### Filebeat 标准输出流采集配置

Filebeat 原生支持容器文件采集，通过add_kubernetes_metadata组件增加kubernetes 元信息，为避免输出组件导致的性能差异，通过drop_event插件丢弃数据，避免输出，filebeat测试配置如下（harvester_buffer_size 调整设置为512K，filebeat.registry.flush: 30s，queue.mem 参数适当扩大，增加吞吐）：

```yaml
  filebeat.yml: |-
    filebeat.registry.flush: 30s
    processors:
      - add_kubernetes_metadata:
          host: ${NODE_NAME}
          matchers:
          - logs_path:
              logs_path: "/var/log/containers/"
      - drop_event:
          when:
            equals:
              input.type: container
    output.console:
      pretty: false
    queue:
      mem:
        events: 4096
        flush.min_events: 2048
        flush.timeout: 1s
    max_procs: 4
    filebeat.inputs:
    - type: container
      harvester_buffer_size: 524288
      paths:
        - /var/log/containers/nginx-log-demo-0-*.log
```

### Filebeat 容器文件采集配置

Filebeat 原生不支持容器内文件采集，因此需要人工将日志打印路径挂载于宿主机HostPath，这里我们使用  subPath以及DirectoryOrCreate功能进行服务打印路径的分离, 以下为模拟不同服务日志打印路径独立情况。
![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537109076-1a362223-c863-4887-996b-c56b90e9f500.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&id=ucbabc4ee&margin=%5Bobject%20Object%5D&name=image.png&originHeight=272&originWidth=3044&originalType=url&ratio=1&rotation=0&showTitle=false&size=419109&status=done&style=none&taskId=u51d1cf1c-27d1-4d8c-a95d-9e0053a8cd1&title=)
filebeat 使用基础日志读取功能读取/testlog路径下的日志，为避免输出组件导致的性能差异，通过drop_event插件丢弃数据，避免输出，测试配置如下（harvester_buffer_size 调整设置为512K，filebeat.registry.flush: 30s，queue.mem 参数适当扩大，增加吞吐）：

```yaml
  filebeat.yml: |-
    filebeat.registry.flush: 30s
    output.console:
      pretty: false
    queue:
      mem:
        events: 4096
        flush.min_events: 2048
        flush.timeout: 1s
    max_procs: 4
    
    filebeat.inputs:
    - type: log
      harvester_buffer_size: 524288
      paths:
        - /testlog/nginx-log-demo-0/*.log
      processors:
        - drop_event:
            when:
              equals:
                log.file.path: /testlog/nginx-log-demo-0/access.log
```

### iLogtail 标准输出流采集配置

iLogtail 原生同样支持标准输出流采集，service_docker_stdout 组件已经会提取kubernetes 元信息，为避免输出组件导致的性能差异，通过processor_filter_regex，进行所有日志的过滤，测试配置如下：

```yaml
{
    "inputs":[
        {
            "detail":{
                "ExcludeLabel":{

                },
                "IncludeLabel":{
                    "io.kubernetes.container.name":"nginx-log-demo-0"
                }
            },
            "type":"service_docker_stdout"
        }
    ],
    "processors":[
        {
            "type":"processor_filter_regex",
            "detail":{
                "Exclude":{
                    "_namespace_":"default"
                }
            }
        }
    ]
}
```

### iLogtail 容器文件采集配置

iLogtail 原生支持容器内文件采集，但由于文件内采集元信息存在于tag标签，暂无过滤插件，为避免输出组件导致的性能差异，因此我们使用空输出插件进行输出，测试配置如下：

```yaml
{
    "metrics":{
        "c0":{
            "advanced":{
                "k8s":{
                    "IncludeLabel":{
                        "io.kubernetes.container.name":"nginx-log-demo-0"
                    }
                }
            },
           ......
            "plugin":{
                "processors":[
                    {
                        "type":"processor_default"
                    }
                ],
                "flushers":[
                    {
                        "type":"flusher_statistics",
                        "detail":{
                            "RateIntervalMs":1000000
                        }
                    }
                ]
            },
            "local_storage":true,
            "log_begin_reg":".*",
            "log_path":"/var/log/medlinker",
            ......
        }
    }
}
```

## Filebeat与iLogtail对比测试

Filebeat 与 iLogtail 的对比项主要包含以下内容：标准输出流采集性能、容器内文件采集性能、标准输出流多用户配置性能、容器内文件多用户配置性能以及大流量采集性能。

### 标准输出流采集性能对比

输入数据源: 283B/s, 底层容器contianerd，标准输出流膨胀后为328B， 共4个输入源：

- 1M/s 输入日志3700条/s,
- 2M/s 输入日志7400条/s,
- 3M/s 输入日志条11100条/s。

​

以下显示了标准输出流不同采集的性能对比，可以看到iLogtail相比于Filebeat 有十倍级的性能优势（CPU的百分比为单核的百分比）：

|  | Filebeat | ILogtail |
| --- | --- | --- |
| 1M/s | 20% | 2% |
| 2M/s | 37% | 3.3% |
| 3M/s | 52% | 4.1% |

以下显示了标准输出流不同采集的内存对比，可以看到logtail 和filebeat 整体内存相差不大，没有出现随采集流量上升内存暴涨情况：

|  | Filebeat | ILogtail |
| --- | --- | --- |
| 1M/s | 136M | 90M |
| 2M/s | 134M | 90M |
| 3M/s | 144M | 90M |

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537108630-b5df6ebb-79fd-4ee9-ad03-066d500fd92f.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=217&id=u8b666ff1&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=24975&status=done&style=none&taskId=uef68e6c0-1f70-4ccd-afb4-64a56646a46&title=&width=362)![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537108684-5cc7cff8-a68b-4a78-835d-d4429455f9a3.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=217&id=uf0f8a5a2&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=22498&status=done&style=none&taskId=ueff2dae5-1662-4006-b50b-524bbb377da&title=&width=362)

### 容器内文件采集性能对比

输入数据源: 283B/s,  共4个输入源：

- 1M/s 输入日志3700条/s,
- 2M/s 输入日志7400条/s,
- 3M/s 输入日志条11100条/s。

以下显示了容器内文件不同采集的性能对比，Filebeat 容器内文件由于与container 采集共用采集组件，并省略了Kubernets meta 相关组件，所以相比于标准输出流采集有大性能提升，iLogtail 的容器内文件采集采用Polling + [inotify](https://man7.org/linux/man-pages/man7/inotify.7.html)机制，同样相比于容器标准输出流采集有性能提升， 但可以看到iLogtail相比于Filebeat 有5倍级的性能优势（CPU的百分比为单核的百分比）：

|  | Filebeat | ILogtail |
| --- | --- | --- |
| 1M/s | 5% | 1.3% |
| 2M/s | 11% | 2% |
| 3M/s | 15% | 3% |

以下显示了标准输出流不同采集的内存对比，可以看到logtail 和filebeat 整体内存相差不大，没有出现随采集流量上升内存暴涨情况：

|  | Filebeat | ILogtail |
| --- | --- | --- |
| 1M/s | 136M | 126M |
| 2M/s | 140M | 141M |
| 3M/s | 141M | 142M |

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537108662-06fc93ab-8f7a-4445-873b-fbd68a21532e.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=217&id=uda491b86&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=26536&status=done&style=none&taskId=uad7154f3-afb4-40fc-8028-f7c4ee01250&title=&width=362)![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537110162-8f28dba5-6e55-4d0e-81fe-5640f2b0d225.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=217&id=u96a02dca&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=23282&status=done&style=none&taskId=u4c18e606-50b4-4b47-8663-6f3baaf0c0b&title=&width=362)

### 采集配置膨胀性能对比

采集配置膨胀性能对比，输入源设置为4，总输入速率为3M/s, 分别进行50采集配置，100采集配置，500采集配置，1000采集配置 对比。

#### 标准输出流采集配置膨胀对比

以下显示了标准输出流不同采集的性能对比，可以看到Filebeat 由于容器采集与静态文件采集底层共用相同静态文件采集逻辑，会在标准输出流采集路径var/log/containers下存在大量正则匹配的工作，可以看到虽然采集数据量没有增加由于采集配置的增加，CPU消耗增加10%+，而iLogtail 针对容器采集模型全局共享容器路径发现机制，所以避免了正则逻辑带来的性能损耗（CPU的百分比为单核的百分比）。

| 采集配置 | Filebeat | ILogtail |
| --- | --- | --- |
| 50 | 65% | 5% |
| 100 | 68% | 6.30% |
| 500 | 75% | 7% |
| 1000 | 82% | 8.70% |

在内存膨胀方面，可以看到不论是Filebeat 还是iLogtail 都存在由于采集配置增加导致的内存膨胀，但2者的膨胀大小都处于可接受范围。

| 采集配置 | Filebeat | ILogtail |
| --- | --- | --- |
| 50 | 148M | 95M |
| 100 | 149M | 98M |
| 500 | 163M | 115M |
| 1000 | 177M | 149M |

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537110426-67cc2eb3-9ee1-4a85-9dc4-b0fb7c945f2f.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=217&id=ua9f978a4&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=26052&status=done&style=none&taskId=ub714d946-c43f-4ae6-beb1-31499cbb131&title=&width=362)![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537110719-bacb6b41-6c46-47c1-96ef-c4c4f38d5980.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=217&id=u6c79dee0&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=24507&status=done&style=none&taskId=u7ae91542-6b57-4d12-841a-d48f99fb5e2&title=&width=362)

#### 容器内文件采集配置膨胀对比

以下显示了容器内文件采集不同采集器的性能对比，可以看到Filebeat 静态文件采集由于避免标准输出流通用路径正则，相较于标准增加CPU消耗较少，而iLogtail CPU 变化同样很小，且相比于标准输出流采集性能略好（CPU的百分比为单核的百分比）。

| 采集配置 | Filebeat | ILogtail |
| --- | --- | --- |
| 50 | 16% | 4.30% |
| 100 | 16.30% | 4.30% |
| 500 | 20.00% | 5.60% |
| 1000 | 22% | 7.30% |

在内存膨胀方面，同样可以看到不论是Filebeat 还是iLogtail 都存在由于采集配置增加导致的内存膨胀，但2者的膨胀大小都处于可接受范围。

| 采集配置 | Filebeat | ILogtail |
| --- | --- | --- |
| 50 | 142M | 99M |
| 100 | 145M | 104M |
| 500 | 164M | 128M |
| 1000 | 183M | 155M |

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537111346-e42487d7-27f7-4fe7-8a02-2fc2fdceb1de.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=217&id=u6cc33de6&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=21701&status=done&style=none&taskId=u6280f78e-b3a3-4f53-a32e-dc954121784&title=&width=362)![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537111701-31115936-caf3-4d7b-a3ae-f82b62c8b2cd.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=217&id=uf56cd907&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=24544&status=done&style=none&taskId=uc83f0fe5-42ad-4375-8c7d-a0a56f87004&title=&width=362)

### iLogtail 采集性能测试

由于FileBeat在日志量大的场景下出现采集延迟问题，所以以下场景仅针对iLogtail进行测试，分别在5M/s、10M/s、20M/s 下针对iLogtail 进行容器标准输出流采集与容器内文件采集的性能压测。

- 输入源数量：10
- 单条日志大小283B
- 5M/s  对应日志速率 18526条/s，单输入源产生速率1852条/s
- 10M/s  对应日志速率 37052条/s，单输入源产生速率3705条/s
- 20M/s  对应日志速率 74104条/s，单输入源产生速率7410条/s
- 40M/s  对应日志速率 148208条/s，单输入源产生速率14820条/s

​

和上述试验类似可以看到CPU消耗方面容器文件采集略好于容器标准输出流采集性能（CPU的百分比为单核的百分比），主要是由于容器文件采集底层Polling + [inotify](https://man7.org/linux/man-pages/man7/inotify.7.html)机制，感兴趣的同学可以阅读[Logtail技术分享一](https://zhuanlan.zhihu.com/p/29303600)了解更多内容。

| 速率 | 容器文件采集 | 标准输出流采集 |
| --- | --- | --- |
| 5M/s | 5.70% | 4.70% |
| 10M/s  | 10.00% | 10.30% |
| 20M/s | 15.00% | 18.70% |
| 40M/s | 22% | 36% |

在内存方面，由于标准输出流采集主要依赖于GO，而容器文件采集主要依赖于C，可以由于GC机制的存在，随着速率的上升，标准输出流采集消耗的内存会逐渐超过容器内文件采集消耗的内存。

| 速率 | 容器文件采集 | 标准输出流采集 |
| --- | --- | --- |
| 5M/s | 148M | 100M |
| 10M/s  | 146M | 130M |
| 20M/s | 147M | 171M |
| 40M/s | 150M | 220M |

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537111882-b3e72905-fc1c-4e45-a814-ed2e9ea233c6.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=220&id=SrpNQ&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=35015&status=done&style=none&taskId=uafd24254-727c-47d3-ba01-346d31db1c6&title=&width=367)![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537112747-026fa4e7-7c6e-4b7c-a202-a42f79e9b392.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=219&id=IrU87&margin=%5Bobject%20Object%5D&name=image.png&originHeight=434&originWidth=724&originalType=binary&ratio=1&rotation=0&showTitle=false&size=27031&status=done&style=none&taskId=uf302707e-c322-44ee-ad9d-014642dd5c4&title=&width=365)

### 对比总结

|  | CPU 对比结论 | 内存对比结论 |
| --- | --- | --- |
| 容器标准输出流采集性能 | 同流量输入下，iLogtail 相较于Filebeat 十倍级别性能 | iLogtail 与Filebeat 内存消耗差异不大。 |
| 容器文件采集采集性能 | 同流量输入下，iLogtail 相较于Filebeat 5倍级别性能 | iLogtail 与Filebeat 内存消耗差异不大。 |
| 容器标准输出流采集多配置性能 | 同流量输入下，随着采集配置增加，Filebeat CPU 增加量为iLogtail CPU增加量的3倍。 | iLogtail 与Filebeat 都会因采集配置增加产生内存膨胀，都处于可接受范围。 |
| 容器文件采集多配置性能 | 同流量输入下，随着采集配置增加，Filebeat CPU 增加量为iLogtail CPU增加量的2倍。 | iLogtail 与Filebeat 都会因采集配置增加产生内存膨胀，都处于可接受范围。 |

## **为什么Filebeat 容器标准输出与文件采集差异巨大？**

通过上述试验可以看到Filebeat 在不同工作模式下有较大的CPU差异，通过dump 容器标准输出流采集的pprof 可以得到如下火焰图，可以看到Filebeat 容器采集下的add_kubernetes_metadata 插件是性能瓶颈，同时Filebeat 的add_kubernetes_metadata 采用与每个节点监听api-server 模式，也存在api-server 压力问题。
![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537116957-d00ae947-476c-45a8-9c14-3335231ac546.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=552&id=ua052289e&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1104&originWidth=2406&originalType=binary&ratio=1&rotation=0&showTitle=false&size=2258463&status=done&style=none&taskId=u20d727bb-d943-4bd9-ae54-c3e76ef8e6d&title=&width=1203)
而iLogtail 取kubernetes meta 完全是兼容kubernetes CRI 协议，直接通过kubernets sandbox 进行meta 数据读取，保证了iLogtail 的高性能采集效率。

### ![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537114447-5ac3bcb9-116d-4bff-b2c0-d55be32ca939.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&id=ud0e5bd3d&margin=%5Bobject%20Object%5D&name=image.png&originHeight=136&originWidth=572&originalType=url&ratio=1&rotation=0&showTitle=false&size=27722&status=done&style=none&taskId=u6e4121cd-0024-42dc-a6cf-411f369c34c&title=)

## **iLogtail DaemonSet 场景优化**

通过以上对比，可以看到iLogtail 相比于Filebeat 具有了优秀的内存以及CPU 消耗，有小伙伴可能好奇iLogtail 拥有如此极致性能背后原因，下文主要讲解iLogtail Daemonset 场景下的优化，如何标准输出流相比于Filebeat 拥有10倍性能，其他具体优化措施请参考：[Logtail技术分享一](https://zhuanlan.zhihu.com/p/29303600)[与 Logtail技术分享二](https://www.sohu.com/a/205324880_465959)。
首先对于标准输出流场景，相比于其他开源采集器，例如Filebeat 或Fluentd。一般都是通过监听var/log/containers**或**/var/log/pods/ ****实现容器标准输出流文件的采集，比如/var/log/pods/ 的路径结构为： /var/log/pods/<namespace>_<pod_name>_<pod_id>/<container_name>/， 通过此路径复用物理机静态文件采集模式进行采集。
![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537114938-4e5607fc-25c3-4059-a9c9-722f402912c6.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&id=u804ad961&margin=%5Bobject%20Object%5D&name=image.png&originHeight=272&originWidth=2268&originalType=url&ratio=1&rotation=0&showTitle=false&size=731953&status=done&style=none&taskId=udbf1b619-feca-4c97-acac-f56eda1c085&title=)
而对于iLogtail，做到了容器化的全支持，iLogtail 通过发现机制，全局维护对Node 节点容器的列表，并实时监听与维护此容器列表。当我们拥有容器列表后，我们便具有了如下优势：

- 采集路径不在依赖于静态配置路径，可以靠容器标签动态选择采集源，从而简化用户接入成本。
- 可以根据容器元信息探测容器自动挂载节点的动态路径，所以iLogtail 无需挂载即可采集容器内文件，而如Filebeat 等采集器需要将容器内路径挂载于宿主机路径，再进行静态文件采集。
- 对于新接入采集配置复用历史容器列表，快速接入采集，而对于空采集配置，由于容器发现全局共享机制的存在，也就避免了存在空轮训监听路径机制的情况，进而保证了在容器这样动态性极高的环境中，iLogtail 可运维性的成本达到可控态。

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1641537116676-044a74eb-8a43-4a49-a19b-70fd0433c79d.png#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&id=ue3871d91&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1606&originWidth=2898&originalType=url&ratio=1&rotation=0&showTitle=false&size=1817942&status=done&style=none&taskId=ud464f5c9-c2f7-43ef-aebb-33efd615b27&title=)

## **结语**

综上所述，在动态性极高的Kubernetes 环境下，iLogtail不会因为采用Daemonset 的部署模型带来的多配置问题，造成内存大幅度膨胀，而且在静态文件采集方面，iLogtail 拥有5倍左右的性能优势，而对于标准输出流采集，由于iLogtail 的采集机制，iLogtail 拥有约10倍左右的性能优势。但是相比于Filebeat 或Fluentd 等老牌开源产品，在文档建设与社区建设上还欠缺很多，欢迎对iLogtail 感兴趣的小伙伴一起参与进来，共同打造易用且高性能的[iLogtail](https://github.com/alibaba/loongcollector) 产品。
![image.png](https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/ilogtail-contact.png?versionId=CAEQOhiBgICQkM6b8xciIDcxZTU5M2FjMDAzODQ1Njg5NjI3ZDc4M2FhOTZkNWNk#clientId=uc89d3166-a47e-4&crop=0&crop=0&crop=1&crop=1&from=paste&id=u5ada78db&margin=%5Bobject%20Object%5D&name=image.png&originHeight=708&originWidth=8014&originalType=url&ratio=1&rotation=0&showTitle=false&size=910470&status=done&style=none&taskId=u41750091-879b-49c9-8d6c-3264841a093&title=)

## **参考文献**

- [Logtail技术分享一](https://zhuanlan.zhihu.com/p/29303600)
- [Logtail技术分享二](https://www.sohu.com/a/205324880_465959)
- [Filebeat 配置](https://www.elastic.co/guide/en/beats/filebeat/current/filebeat-input-container.html)
- [Filebeat 容器化部署](https://www.elastic.co/guide/en/beats/filebeat/current/running-on-kubernetes.html)
