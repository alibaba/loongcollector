# 无侵入网络调用数据

## 简介

`observer_ilogtail_network` 插件支持从网络系统调用中收集四层网络调用，并借助网络解析模块，可以识别七层网络调用细节。此功能核心网络观测能力基于eBPF技术实现，具有高性能、无侵入等核心优势。

## 版本

[Beta](../../stability-level.md)

## 前提

1. Linux 内核 4.19+
2. 下载libebpf.so 动态链接库放到iLogtail 的运行路径，此代码后续被CoolBpf 开源。
   - `wget https://logtail-release-us-west-1.oss-us-west-1.aliyuncs.com/kubernetes/libebpf.so`

## 配置参数

### 基础参数

| 参数                          | 类型              | 是否必选 | 说明                                                         |
|-----------------------------| ----------------- | -------- |------------------------------------------------------------|
| Type                        | String            | 否       | 插件类型，指定为`observer_ilogtail_network_v1`。                    |
| Common.FlushOutL4Interval   | Number            | 否       | 聚合数据输出间隔时间，默认15s                                           |
| Common.FlushOutL7Interval   | Number            | 否       | 聚合数据输出间隔时间，默认60s                                           |
| Common.FlushMetaInterval    | Number            | 否       | 读取容器元信息间隔时间，默认30s，行为可类比我为docker ps                         |
| Common.FlushNetlinkInterval | Number            | 否       | 设置读取Socket元信息的时间间隔，默认10s                                   |
| Common.Sampling             | Number            | 否       | 设置网络数据的采样率，默认100                                           |
| Common.ProtocolProcess      | bool              | 否       | 开启后Logtail将解析应用层的网络协议数据，例如HTTP、MySQL、Redis等，默认开启。          |
| Common.DropUnixSocket       | Number            | 否       | 开启后将丢弃Unix域网络请求。Unix域常用于本地网络交互，默认开启。                       |
| Common.DropLocalConnections | Number            | 否       | 开启后丢弃对端地址为本地的INET域网络请求，默认开启。                               |
| Common.DropUnknownSocket    | Number            | 否       | 开启后将丢弃非INET域或Unix域的网络请求，默认开启。                              |
| Common.IncludeProtocols     | []string          | 否       | 进行7层网络协议识别的协议类别，默认为全部，目前支持HTTP、Redis、MySQL、PgSQL、DNS 5种协议。 |
| Common.Tags                 | map[string]string | 否       | tagb标签，会被附带上传                                              |
| EBPF.Enabled                | map[string]string | 是       | 开启Ebpf 功能                                                  |

### 高级参数

| 参数                             | 类型              | 是否必选 | 说明                                                                                                                                              |
| -------------------------------- | ----------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| Common.IncludeContainerLabels    | map[string]string | 否       | 用于指定待采集的容器。多个白名单之间为或关系，即只要容器Label满足任一白名单即可被匹配。注意： 此变量为容器Label而非Kubernetes Label               |
| Common.ExcludeContainerLabels    | map[string]string | 否       | 用于排除待采集的容器。多个黑名单之间为或关系，即只要容器Label满足任一黑名单即可被匹配。注意： 此变量为容器Label而非Kubernetes Label               |
| Common.IncludeK8sLabels          | map[string]string | 否       | 用于指定待采集的容器。设置LabelKey为具体名称，LabelValue为正则表达式。例如设置LabelKey为io.kubernetes.container.name，设置LabelValue为^(nginx     |
| Common.ExcludeK8sLabels          | map[string]string | 否       | 用于排除不需要采集的容器。设置LabelKey为具体名称，LabelValue为正则表达式。例如设置LabelKey为io.kubernetes.container.name，设置LabelValue为^(nginx |
| Common.IncludeEnvs               | map[string]string | 否       | 用于指定待采集的容器。设置EnvKey为具体名称，EnvValue为正则表达式。例如设置EnvKey为NGINX_SERVICE_PORT，设置EnvValue为^(80                          |
| Common.ExcludeEnvs               | map[string]string | 否       | 用于排除不需要采集的容器。设置EnvKey为具体名称，EnvValue为正则表达式。例如设置EnvKey为NGINX_SERVICE_PORT，设置EnvValue为^(80                      |
| Common.IncludeCmdRegex           | string            | 否       | 设置命令行正则表达式，用于指定需要监控的进程。例如设置为^mockclient$，表示仅监控/proc/{pid}/cmdline下名为mockclient的进程。                       |
| Common.ExcludeCmdRegex           | string            | 否       | 设置命令行正则表达式，用于排除不需要监控的进程。例如设置为^mockclient$，表示不监控/proc/{pid}/cmdline下名为mockclient的进程。                     |
| Common.IncludeContainerNameRegex | string            | 否       | 输入匹配Contianer名称的正则表达式，用于指定待采集的 Contianer。                                                                                   |
| Common.ExcludeContainerNameRegex | string            | 否       | 输入匹配Contianer名称的正则表达式，用于排除不需要采集的 Contianer。                                                                               |
| Common.IncludePodNameRegex       | string            | 否       | 输入匹配Pod名称的正则表达式，用于指定待采集的Pod。                                                                                                |
| Common.ExcludePodNameRegex       | string            | 否       | 输入匹配Pod名称的正则表达式，用于排除不需要采集的Pod。                                                                                            |
| Common.IncludeNamespaceNameRegex | string            | 否       | 输入匹配Namespace名称的正则表达式，用于指定待采集的命名空间。                                                                                     |
| Common.ExcludeNamespaceNameRegex | string            | 否       | 输入匹配Namespace名称的正则表达式，用于排除不需要采集的命名空间。                                                                                 |
| EBPF.Pid                         | map[string]string | 否       | 指定唯一的进程ID 用于采集范围确定                                                                                                                 |

## 样例：收集HTTP 请求调用

### 采集配置

*observer_ilogtail_network_v1 程序运行与C++ 核心部分，需要增加processor_default 插件，才可以选取社区多种输出插件，如Kafka*

```yaml
enable: true
inputs:
  - Type: observer_ilogtail_network_v1
    Common:
      FlushOutL4Interval: 5
      FlushOutL7Interval: 5
    EBPF:
      Enabled: true
processors:
  - Type: processor_default
flushers:
  - Type: flusher_stdout
```

#### 输出

```plain
2022-08-25 16:44:01 [INF] [flusher_stdout.go:119] [Flush] [config#/root/workspace/ilogtail/output/./config/local/demo.yaml,] {"type":"protocols","_process_pid_":"605","_process_cmd_":"/usr/sbin/nscd","_running_mode_":"host","protocol":"dns","query_record":"ilogtail-community-edition.cn-shanghai.log.aliyuncs.com","query_type":"A","success":"1","role":"client","remote_info":"{\"remote_ip\":\"100.100.2.136\",\"remote_port\":\"53\",\"remote_type\":\"dns\"}\n","total_count":"1","total_latency_ns":"387138","total_req_bytes":"73","total_resp_bytes":"272","__time__":"1661417038"}:
2022-08-25 16:44:01 [INF] [flusher_stdout.go:119] [Flush] [config#/root/workspace/ilogtail/output/./config/local/demo.yaml,] {"type":"protocols","_process_pid_":"1945","_process_cmd_":"/usr/local/cloudmonitor/bin/argusagent","_running_mode_":"host","protocol":"http","version":"1","host":"metrichub-cn-beijing.aliyun.com","method":"POST","url":"/agent/metrics/putLines","resp_code":"200","role":"client","remote_info":"{\"remote_ip\":\"100.100.105.70\",\"remote_port\":\"80\",\"remote_type\":\"server\"}\n","total_count":"1","total_latency_ns":"3492453","total_req_bytes":"13661","total_resp_bytes":"165","__time__":"1661417038"}:
2022-08-25 16:44:01 [INF] [flusher_stdout.go:119] [Flush] [config#/root/workspace/ilogtail/output/./config/local/demo.yaml,] {"type":"statistics","_process_pid_":"1945","_process_cmd_":"/usr/local/cloudmonitor/bin/argusagent","_running_mode_":"host","remote_info":"{\"remote_ip\":\"100.100.105.70\",\"remote_port\":\"80\",\"remote_type\":\"server\"}\n","socket_type":"inet_socket","role":"client","send_bytes":"13661","recv_bytes":"165","send_packets":"1","recv_packets":"1","send_total_latency":"0","recv_total_latency":"0","__time__":"1661417038"}:
2022-08-25 16:44:01 [INF] [flusher_stdout.go:119] [Flush] [config#/root/workspace/ilogtail/output/./config/local/demo.yaml,] {"type":"statistics","_process_pid_":"605","_process_cmd_":"/usr/sbin/nscd","_running_mode_":"host","remote_info":"{\"remote_ip\":\"100.100.2.136\",\"remote_port\":\"53\",\"remote_type\":\"dns\"}\n","socket_type":"inet_socket","role":"client","send_bytes":"73","recv_bytes":"272","send_packets":"1","recv_packets":"1","send_total_latency":"0","recv_total_latency":"0","__time__":"1661417038"}:
```
