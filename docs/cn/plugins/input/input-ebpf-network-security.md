# input_ebpf_network_security 插件

## 简介

`input_ebpf_network_security`插件可以实现利用ebpf探针采集网络安全相关动作。

## 版本

[Dev](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_ebpf\_network\_security  |
|  ProbeConfig  |  \[object\]  |  否  |  ProbeConfig 默认包含一个 Option，其中包含一个默认取全部值的 CallNameFilter，其他 Filter 默认为空  |  ProbeConfig 可以包含多个 Option， Option 内部有多个 Filter，Filter 内部是或的关系，Filter 之间是且的关系，Option 之间是或的关系  |
|  ProbeConfig[xx].CallNameFilter  |  \[string\]  |  否  |  该插件支持的所有 callname: [ tcp_connect tcp_close tcp_sendmsg ]  |  内核挂载点过滤器，按照白名单模式运行，不填表示配置该插件所支持的所有挂载点  |
|  ProbeConfig[xx].AddrFilter  |  object  |  否  |  /  |  网络地址过滤器  |
|  ProbeConfig[xx].AddrFilter.DestAddrList  |  \[string\]  |  否  |  空  |  目的IP地址白名单，不填表示不进行过滤  |
|  ProbeConfig[xx].AddrFilter.DestPortList  |  \[string\]  |  否  |  空  |  目的端口白名单，不填表示不进行过滤  |
|  ProbeConfig[xx].AddrFilter.DestAddrBlackList  |  \[string\]  |  否  |  空  |  目的IP地址黑名单，不填表示不进行过滤  |
|  ProbeConfig[xx].AddrFilter.DestPortBlackList  |  \[string\]  |  否  |  空  |  目的端口黑名单，不填表示不进行过滤  |
|  ProbeConfig[xx].AddrFilter.SourceAddrList  |  \[string\]  |  否  |  空  |  源IP地址白名单，不填表示不进行过滤  |
|  ProbeConfig[xx].AddrFilter.SourcePortList  |  \[string\]  |  否  |  空  |  源端口白名单，不填表示不进行过滤  |
|  ProbeConfig[xx].AddrFilter.SourceAddrBlackList  |  \[string\]  |  否  |  空  |  源IP地址黑名单，不填表示不进行过滤  |
|  ProbeConfig[xx].AddrFilter.SourcePortBlackList  |  \[string\]  |  否  |  空  |  源端口黑名单，不填表示不进行过滤  |

## 样例

### XXXX

* 输入

```json
TODO
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_ebpf_sockettraceprobe_security
    ProbeConfig:
      - CallNameFilter: 
        - "tcp_connect"
        - "tcp_close"
        AddrFilter: 
          DestAddrList: 
            - "10.0.0.0/8"
            - "92.168.0.0/16"
          DestPortList: 
            - 80
          SourceAddrBlackList: 
            - "127.0.0.1/8"
          SourcePortBlackList: 
            - 9300
      - CallNameFilter: 
        - "tcp_sendmsg"
        AddrFilter: 
          DestAddrList: 
            - "10.0.0.0/8"
            - "92.168.0.0/16"
          DestPortList: 
            - 80
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
TODO
```
