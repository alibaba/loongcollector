# iuput_ebpf_file_security 插件

## 简介

`iuput_ebpf_file_security`插件可以实现利用ebpf探针采集文件安全相关动作。

## 版本

[Alpha](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为iuput\_ebpf\_file\_security  |
|  ProbeConfig  |  \[object\]  |  是  |  /  |  插件配置参数列表  |
|  ProbeConfig.CallName  |  \[string\]  |  否  |  空  |  系统调用函数  |
|  ProbeConfig.FilePathFilter  |  \[object\]  |  是  |  /  |  过滤参数  |
|  ProbeConfig.FilePathFilter.FilePath  |  string  |  是  |  /  |  文件路径  |
|  ProbeConfig.FilePathFilter.FileName  |  string  |  否  |  空  |  文件名。不填 FileName 代表采集对应的 FilePath 下所有文件  |

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
      - CallName: 
        - "security_file_permission"
        FilePathFilter: 
          - FilePath: "/etc/"
            FileName: "passwd"
          - FilePath: "/lib"
      - CallName: 
        - "security_path_truncate"
        FilePathFilter: 
          - FilePath: "/etc/"
            FileName: "passwd"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
TODO
```
