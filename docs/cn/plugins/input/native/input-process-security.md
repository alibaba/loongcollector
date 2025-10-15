# input_process_security 插件

## 简介

`input_process_security`插件可以实现利用ebpf探针采集进程安全相关动作。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.1.6 及以上

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_process\_security  |

## 输出格式

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| exec\_id | string | 执行id。一个进程内的所有触发事件使用同一个exec\_id，记录进程内所有关联的活动。 |
| pid | string | 进程id |
| uid | string | 账号id |
| user | string | 进程运行的用户信息 |
| call\_name | string | 系统调用函数名，可选值：execve（进程执行）、exit（进程退出）等 |
| binary | string | 执行的命令，二进制可执行文件路径，如/usr/bin/python3等 |
| arguments | string | 进程参数 |
| cwd | string | 当前工作路径 |
| cap.permitted | string | 许可能力集（capabilities），定义进程可执行的权限集合 |
| cap.effective | string | 有效能力集，进程当前拥有的有效权限集合 |
| cap.inheritable | string | 可继承能力集，子进程可继承的权限集合 |
| parent.exec\_id | string | 父进程执行id |
| parent.pid | string | 父进程id |
| parent.uid | string | 父进程账号id |
| parent.user | string | 父进程运行的用户信息 |
| parent.binary | string | 父进程可执行文件路径 |
| parent.arguments | string | 父进程参数 |
| parent.cwd | string | 父进程工作路径 |
| exit\_code | string | 进程退出码（仅在exit事件中出现） |
| exit\_tid | string | 退出线程id（仅在exit事件中出现） |
| \_\_time\_\_ | int64 | 事件发生时间戳 |

### K8s

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| k8s.namespace | string | K8s命名空间 |
| k8s.pod.name | string | K8s Pod名称 |
| k8s.container.name | string | K8s资源配置中的容器名称 |
| k8s.workload.name | string | K8s 负载名称 |
| k8s.workload.kind | string | K8s 负载类型 |

### Container

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| container.id | string | 容器id |
| container.name | string | 本地容器名 |
| container.image.id | string | 镜像id |
| container.image.name | string | 镜像name |

## 样例

### 采集进程安全数据

* 输入

``` shell
python3 -m http.server 8000
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_process_security
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
{
    "exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDYzOTcwNzQ4MDA2ODI4MDoxNDg3NDk=",
    "pid": "148749",
    "uid": "3000",
    "user": "user",
    "binary": "/usr/bin/python3",
    "arguments": "-m http.server 8000",
    "cwd": "/workspace/loongcollector_community_test",
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
    "call_name": "execve",
    "event_type": "kprobe",
    "__time__": "1758701451"
}
{
    "exec_id": "djQzYzExMjA0LnNxYS5uYTEzMTozNDYzOTcwNzQ4MDA2ODI4MDoxNDg3NDk=",
    "pid": "148749",
    "uid": "3000",
    "user": "user",
    "binary": "/usr/bin/python3",
    "arguments": "-m http.server 8000",
    "cwd": "/workspace/loongcollector_community_test",
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
    "call_name": "exit",
    "event_type": "kprobe",
    "exit_code": "0",
    "exit_tid": "148749",
    "__time__": "1758701453"
}
```
