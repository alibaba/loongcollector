# iuput_ebpf_process_security 插件

## 简介

`iuput_ebpf_process_security`插件可以实现利用ebpf探针采集进程安全相关动作。

## 版本

[Dev](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为iuput\_ebpf\_process\_security  |
|  ProbeConfig  |  \[object\]  |  是  |  /  |  插件配置参数列表  |
|  ProbeConfig.CallName  |  \[string\]  |  否  |  空  |  系统调用函数  |
|  ProbeConfig.NamespaceFilter  |  object  |  否  |  空  |  命名空间  |
|  ProbeConfig.NamespaceBlackFilter  |  object  |  否  |  空  |  命名空间  |
|  ProbeConfig.Namespace\[Black\]Filter.NamespaceType  |  string  |  是  |  /  |  命名空间类型 \[范围：Uts, Ipc, Mnt, Pid, PidForChildren, Net, Cgroup, User, Time, TimeForChildren\] |
|  ProbeConfig.Namespace\[Black\]Filter.ValueList  |  \[string\]  |  是  |  /  |  特定命名空间类型对应的取值列表 |

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
  - Type: input_ebpf_processprobe_security
    ProbeConfig:
      NamespaceFilter:
        - NamespaceType: "Pid"
          ValueList: 
            - "4026531833"
        - NamespaceType: "Mnt"
          ValueList: 
            - "4026531834"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
TODO
```
