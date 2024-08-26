# input_ebpf_process_security 插件

## 简介

`input_ebpf_process_security`插件可以实现利用ebpf探针采集进程安全相关动作。

## 版本

[Dev](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_ebpf\_process\_security  |
|  ProbeConfig  |  \[object\]  |  否  |  ProbeConfig 默认包含一个 Option，其中包含一个默认取全部值的 CallNameFilter，其他 Filter 默认为空  |  ProbeConfig 可以包含多个 Option， Option 内部有多个 Filter，Filter 内部是或的关系，Filter 之间是且的关系，Option 之间是或的关系  |
|  ProbeConfig[xx].CallNameFilter  |  \[string\]  |  否  |  该插件支持的所有 callname: [ sys_enter_execve sys_enter_clone disassociate_ctty acct_process wake_up_new_task ]  |  内核挂载点过滤器，按照白名单模式运行，不填表示配置该插件所支持的所有挂载点  |

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
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
TODO
```
