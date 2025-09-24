# input_network_security 插件

## 简介

`input_network_security`插件可以实现利用ebpf探针采集网络安全相关动作。

## 版本

[Dev](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_network\_security  |
|  ProbeConfig  |  object  |  否  |  ProbeConfig 包含默认为空的 Filter  |  ProbeConfig 内部包含 Filter，Filter 内部是或的关系  |
|  ProbeConfig.AddrFilter  |  object  |  否  |  /  |  网络地址过滤器  |
|  ProbeConfig.AddrFilter.DestAddrList  |  []string  |  否  |  空  |  目的IP地址白名单，不填表示不进行过滤  |
|  ProbeConfig.AddrFilter.DestPortList  |  []uint  |  否  |  空  |  目的端口白名单，不填表示不进行过滤  |
|  ProbeConfig.AddrFilter.DestAddrBlackList  |  []string  |  否  |  空  |  目的IP地址黑名单，不填表示不进行过滤  |
|  ProbeConfig.AddrFilter.DestPortBlackList  |  []uint  |  否  |  空  |  目的端口黑名单，不填表示不进行过滤  |
|  ProbeConfig.AddrFilter.SourceAddrList  |  []string  |  否  |  空  |  源IP地址白名单，不填表示不进行过滤  |
|  ProbeConfig.AddrFilter.SourcePortList  |  []uint  |  否  |  空  |  源端口白名单，不填表示不进行过滤  |
|  ProbeConfig.AddrFilter.SourceAddrBlackList  |  []string  |  否  |  空  |  源IP地址黑名单，不填表示不进行过滤  |
|  ProbeConfig.AddrFilter.SourcePortBlackList  |  []uint  |  否  |  空  |  源端口黑名单，不填表示不进行过滤  |

## 输出格式

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| call\_name | string | 系统调用函数名。可选值：tcp_connect（连接建立）、tcp_close（连接断开）、tcp_sendmsg（发送数据）等 |
| net.saddr | string | 源IP |
| net.daddr | string | 目标IP |
| net.sport | string | 源port |
| net.dport | string | 目标port |
| net.status | string | 状态：TCP\_SYN\_SENT、TCP\_ESTABLISHED |
| net.bytes | string | 发送的真实的字节数，仅tcp\_sendmsg有效 |
| net.protocol | string | 网络协议，如 TCP 或 UDP。 |

其他字段参见 [input_process_security 插件](input-process-security.md)。

## 样例

### 采集网络安全事件

* 输入

```json
curl www.alibabacloud.com
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_network_security
    ProbeConfig:
      AddrFilter: 
        DestAddrList: 
          - "47.74.138.0/24"
          - "47.241.205.0/24"
        DestPortList: 
          - 80
          - 443
        SourceAddrBlackList: 
          - "127.0.0.1/8"
        SourcePortBlackList: 
          - 9300
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
{
    "exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDY1MzQzNDQxMjM2MDI3MDo3NTQyNDU=",
    "pid": "754245",
    "uid": "3000",
    "user": "user",
    "binary": "/usr/bin/curl",
    "arguments": "www.alibabacloud.com",
    "cwd": "/workspace/loongcollector_community_test",
    "ktime": "34653434412360270",
    "cap.permitted": "",
    "cap.effective": "",
    "cap.inheritable": "",
    "parent.exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDYxODUyNzQ4MDAwMDAwMDozNDE2MTM2",
    "parent.pid": "3416136",
    "parent.uid": "3000",
    "parent.user": "user",
    "parent.binary": "/usr/bin/bash",
    "parent.arguments": "",
    "parent.cwd": "/workspace/loongcollector_community_test",
    "parent.ktime": "34618527480000000",
    "network.protocol": "TCP",
    "family": "AF_INET",
    "network.saddr": "100.82.116.88",
    "network.daddr": "47.74.138.67",
    "network.sport": "44332",
    "network.dport": "80",
    "netns": "4026532058",
    "call_name": "tcp_connect",
    "event_type": "kprobe",
    "__time__": "1758715178"
}
{
    "exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDY1MzQzNDQxMjM2MDI3MDo3NTQyNDU=",
    "pid": "754245",
    "uid": "3000",
    "user": "user",
    "binary": "/usr/bin/curl",
    "arguments": "www.alibabacloud.com",
    "cwd": "/workspace/loongcollector_community_test",
    "ktime": "34653434412360270",
    "cap.permitted": "",
    "cap.effective": "",
    "cap.inheritable": "",
    "parent.exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDYxODUyNzQ4MDAwMDAwMDozNDE2MTM2",
    "parent.pid": "3416136",
    "parent.uid": "3000",
    "parent.user": "user",
    "parent.binary": "/usr/bin/bash",
    "parent.arguments": "",
    "parent.cwd": "/workspace/loongcollector_community_test",
    "parent.ktime": "34618527480000000",
    "network.protocol": "TCP",
    "family": "AF_INET",
    "network.saddr": "100.82.116.88",
    "network.daddr": "47.74.138.67",
    "network.sport": "44332",
    "network.dport": "80",
    "netns": "4026532058",
    "call_name": "tcp_sendmsg",
    "event_type": "kprobe",
    "__time__": "1758715178"
}
{
    "exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDY1MzQzNDQxMjM2MDI3MDo3NTQyNDU=",
    "pid": "754245",
    "uid": "3000",
    "user": "user",
    "binary": "/usr/bin/curl",
    "arguments": "www.alibabacloud.com",
    "cwd": "/workspace/loongcollector_community_test",
    "ktime": "34653434412360270",
    "cap.permitted": "",
    "cap.effective": "",
    "cap.inheritable": "",
    "parent.exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDYxODUyNzQ4MDAwMDAwMDozNDE2MTM2",
    "parent.pid": "3416136",
    "parent.uid": "3000",
    "parent.user": "user",
    "parent.binary": "/usr/bin/bash",
    "parent.arguments": "",
    "parent.cwd": "/workspace/loongcollector_community_test",
    "parent.ktime": "34618527480000000",
    "network.protocol": "TCP",
    "family": "AF_INET",
    "network.saddr": "100.82.116.88",
    "network.daddr": "47.74.138.67",
    "network.sport": "44332",
    "network.dport": "80",
    "netns": "4026532058",
    "call_name": "tcp_close",
    "event_type": "kprobe",
    "__time__": "1758715178"
}
```
