# 主机监控数据

## 简介

`metric_system_v2` `input`插件用于采集主机的监控数据（例如CPU、内存、负载、磁盘、网络等）。

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数          | 类型      | 是否必选 | 说明                                                                                         |
| ----------- | ------- | ---- | ------------------------------------------------------------------------------------------ |
| Type        | String  | 是    | 插件类型                                                                                       |
| CPU     | Boolean  | 否    | <p>是否开启主机CPU指标采集。</p><p>默认取值：true。</p> |
| Mem    | Boolean  | 否   | <p>是否开启主机Memory指标采集。</p><p>默认取值：true。</p> |
| Disk    | Boolean | 否    | <p>是否开启主机Disk指标采集。</p><p>默认取值：true。</p> |
| Net    | Boolean | 否    | <p>是否开启主机Net指标采集。</p><p>默认取值：true。</p> |
| Protocol  | Boolean | 否    | <p>是否开启主机Protocol指标采集。</p><p>默认取值：true。</p>件。                                                                               |
| TCP    | Boolean | 否    | <p>是否开启主机Net采集。</p><p>默认取值：false。</p> |
| OpenFd    | Boolean | 否    | <p>是否开启主机Net采集。</p><p>默认取值：true。</p> |
| CPUPercent    | Boolean | 否    | <p>是否开启主机CPU使用率指标采集。</p><p>默认取值：true。</p> |
| Labels  | String字典 | 否    | 自定义Labels。件。                                                                               |

## 样例

采集主机上CPU、Memory、Net、Disk、Process等监控数据。

### 采集配置

```yaml
enable: true
inputs:
  - Type: metric_system_v2
    CPU: true
    Mem: true
    Disk: true
    Disk: true
    Net: true
    Protocol: true
    TCP: true
    OpenFd: true
    CPUPercent: true
    Labels:
      cluster: ilogtail-test-cluster
flushers:
  - Type: flusher_stdout
    FileName: /tmp/124.log
    OnlyStdout: false
```

#### 输出

* 主机指标信息

```json
{
    "__name__":"net_out_pkt",
    "__labels__":"cluster#$#ilogtail-test-cluster|hostname#$#master-1-1.c-ca9717110efa1b40|hostname#$#test-1|interface#$#eth0|ip#$#10.1.37.31",
    "__time_nano__":"1680079323040664058",
    "__value__":"32.764761658490045",
    "__time__":"1680079323"
}
```
