# input_ebpf_file_security 插件

## 简介

`input_ebpf_file_security`插件可以实现利用ebpf探针采集文件安全相关动作。

## 版本

[Dev](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_ebpf\_file\_security  |
|  ProbeConfig  |  \[object\]  |  否  |  ProbeConfig 默认包含一个 Option，其中包含一个默认取全部值的 CallNameFilter，其他 Filter 默认为空  |  ProbeConfig 可以包含多个 Option， Option 内部有多个 Filter，Filter 内部是或的关系，Filter 之间是且的关系，Option 之间是或的关系  |
|  ProbeConfig[xx].CallNameFilter  |  \[string\]  |  否  |  该插件支持的所有 callname: [ security_file_permission security_mmap_file security_path_truncate ]  |  内核挂载点过滤器，按照白名单模式运行，不填表示配置该插件所支持的所有挂载点  |
|  ProbeConfig[xx].FilePathFilter  |  \[string\]  |  否  |  空  |  文件路径过滤器，按照白名单模式运行，不填表示不进行过滤  |

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
  - Type: input_ebpf_fileprobe_security
    ProbeConfig:
      - CallNameFilter: 
        - "security_file_permission"
        FilePathFilter: 
          - "/etc/passwd"
          - "/lib"
      - CallNameFilter: 
        - "security_path_truncate"
        FilePathFilter: 
          - "/etc/passwd"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
TODO
```
