# input_file_security 插件

## 简介

`input_file_security`插件可以实现利用ebpf探针采集文件安全相关动作。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.1.6 及以上

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_file\_security  |
|  ProbeConfig  |  object  |  否  |  ProbeConfig 包含默认为空的 Filter  |  ProbeConfig 内部包含 Filter，Filter 内部是或的关系  |
|  ProbeConfig.FilePathFilter  |  []string  |  否  |  空  |  文件路径前缀过滤器，按照白名单模式运行，不填表示不进行过滤  |

## 输出格式

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| call\_name | string | 系统调用函数名。可选值：read（文件读取操作）、write（文件写入操作）、security_file_permission（文件权限变更相关操作）、security_path_truncate（文件截断）、security_mmap_file：文件被内存映射等 |
| file.path | string | 文件完整路径 |

其他字段参见 [input_process_security 插件](input-process-security.md)。

## 样例

### 采集文件安全事件

* 输入

``` shell
sudo bash -c 'echo >> /etc/pam.conf; echo >> /etc/pam.d/su'
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file_security
    ProbeConfig:
      FilePathFilter: 
        - "/etc/pam.conf"
        - "/etc/pam.d"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
{
    "exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDYzNzY3NjYzNjg4MDYwMzo1OTgxMg==",
    "pid": "59812",
    "uid": "0",
    "user": "root",
    "binary": "/usr/bin/bash",
    "arguments": "-c \"echo >> /etc/pam.conf; echo >> /etc/pam.d/su\"",
    "cwd": "/workspace/loongcollector_community_test",
    "cap.permitted": "CAP_CHOWN DAC_OVERRIDE CAP_DAC_READ_SEARCH CAP_FOWNER CAP_FSETID CAP_KILL CAP_SETGID CAP_SETUID CAP_SETPCAP CAP_LINUX_IMMUTABLE CAP_NET_BIND_SERVICE CAP_NET_BROADCAST CAP_NET_ADMIN CAP_NET_RAW CAP_IPC_LOCK CAP_IPC_OWNER CAP_SYS_MODULE CAP_SYS_RAWIO CAP_SYS_CHROOT CAP_SYS_PTRACE CAP_SYS_PACCT CAP_SYS_ADMIN CAP_SYS_BOOT CAP_SYS_NICE CAP_SYS_RESOURCE CAP_SYS_TIME CAP_SYS_TTY_CONFIG CAP_MKNOD CAP_LEASE CAP_AUDIT_WRITE CAP_AUDIT_CONTROL CAP_SETFCAP CAP_MAC_OVERRIDE CAP_MAC_ADMIN CAP_SYSLOG CAP_WAKE_ALARM CAP_BLOCK_SUSPEND CAP_AUDIT_READ CAP_PERFMON CAP_BPF CAP_CHECKPOINT_RESTORE",
    "cap.effective": "CAP_CHOWN DAC_OVERRIDE CAP_DAC_READ_SEARCH CAP_FOWNER CAP_FSETID CAP_KILL CAP_SETGID CAP_SETUID CAP_SETPCAP CAP_LINUX_IMMUTABLE CAP_NET_BIND_SERVICE CAP_NET_BROADCAST CAP_NET_ADMIN CAP_NET_RAW CAP_IPC_LOCK CAP_IPC_OWNER CAP_SYS_MODULE CAP_SYS_RAWIO CAP_SYS_CHROOT CAP_SYS_PTRACE CAP_SYS_PACCT CAP_SYS_ADMIN CAP_SYS_BOOT CAP_SYS_NICE CAP_SYS_RESOURCE CAP_SYS_TIME CAP_SYS_TTY_CONFIG CAP_MKNOD CAP_LEASE CAP_AUDIT_WRITE CAP_AUDIT_CONTROL CAP_SETFCAP CAP_MAC_OVERRIDE CAP_MAC_ADMIN CAP_SYSLOG CAP_WAKE_ALARM CAP_BLOCK_SUSPEND CAP_AUDIT_READ CAP_PERFMON CAP_BPF CAP_CHECKPOINT_RESTORE",
    "cap.inheritable": "",
    "parent.exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDYzNzY3NjYyNTczNDkyNjo1OTgxMQ==",
    "parent.pid": "59811",
    "parent.uid": "0",
    "parent.user": "root",
    "parent.binary": "/usr/bin/sudo",
    "parent.arguments": "bash -c \"echo >> /etc/pam.conf; echo >> /etc/pam.d/su\"",
    "parent.cwd": "/workspace/loongcollector_community_test",
    "file.path": "/etc/pam.conf",
    "call_name": "write",
    "event_type": "kprobe",
    "__time__": "1758699420"
}
```
