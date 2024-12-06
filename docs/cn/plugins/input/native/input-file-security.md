# input_file_security 插件

## 简介

`input_file_security`插件可以实现利用ebpf探针采集文件安全相关动作。

## 版本

[Dev](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_file\_security  |
|  ProbeConfig  |  object  |  否  |  ProbeConfig 包含默认为空的 Filter  |  ProbeConfig 内部包含 Filter，Filter 内部是或的关系  |
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
  - Type: input_file_security
    ProbeConfig:
      FilePathFilter: 
        - "/etc/passwd"
        - "/lib"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
TODO
```
