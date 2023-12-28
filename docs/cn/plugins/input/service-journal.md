# Journal数据

## 简介

`service_journal` 插件支持从原始的二进制文件中采集Linux系统的Journal（systemd）日志。systemd是专用于 Linux 操作系统的系统与服务管理器。当作为启动进程(PID=1)运行时，它将作为初始化系统运行，启动并维护各种用户空间的服务。 systemd统一管理所有Unit的日志（包括内核和应用日志），配置文件一般在`/etc/systemd/journald.conf`中。[源代码](https://github.com/alibaba/ilogtail/blob/main/plugins/input/journal/input_journal.go)

## 版本

[Stable](../stability-level.md)

### 功能

* 支持设置初始同步位置，后续采集会自动保存checkpoint，不需关心应用重启。
* 支持过滤指定的Unit。
* 支持采集内核日志。
* 支持自动解析日志等级。
* 支持以容器方式采集宿主机上的Journal日志，适用于Docker/Kubernetes场景。

### 相关限制

* Logtail支持版本为0.16.18及以上。
* 运行的操作系统需支持Journal日志格式。

### 适用场景

1. 监听内核事件，出现异常时自动告警。
2. 采集所有系统日志，用于长期存储，减少磁盘空间占用。
3. 采集软件（Unit）的输出日志，用于分析或告警。
4. 采集所有Journal日志，可以从所有日志中快速检索关键词或日志，相比Journalctl查询大幅提升。

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type    | String，无默认值（必填） | 插件类型，固定为`service_journal`      |
| JournalPaths | Array，其中value为String，无默认值（必填） | Journal日志路径，建议直接填写Journal日志所在文件夹，例如`/var/log/journal`。 |
| SeekPosition | String，`tail` | 首次采集方式，为head则采集所有数据，为tail则只采集配置应用后新的数据。 |
| Kernel | Boolean，`true` | 为false时则不采集内核日志。 |
| Units | Array，其中value为String，`[]` | 指定采集的Unit列表，为空时则全部采集。 |
| ParseSyslogFacility | Boolean，`false` | 是否解析syslog日志的facility字段。 |
| ParsePriority | Boolean，`false` | 是否解析Priority字段。|
| UseJournalEventTime | Boolean，`false` | 是否使用Journal日志中的字段作为日志时间，即使用采集时间作为日志时间（实时日志采集一般相差3秒以内）。|
| CursorFlushPeriodMs | Integer，`5000` | 日志读取检查点的刷新时间。 |
| CursorSeekFallback | String，`SeekPositionTail` | 日志读取检查点回退的位置。 |
| Identifiers | Array，其中value为String，`[]` | syslog标识符，可以添加到监视器。 |
| MatchPatterns | Array，其中value为String，`[]` | 匹配规则，可以添加到监视器。 |

ParsePriority映射关系表如下：

```go
 "0": "emergency"
 "1": "alert"
 "2": "critical"
 "3": "error"
 "4": "warning"
 "5": "notice"
 "6": "informational"
 "7": "debug"
```

## 样例

### 样例1

从默认的`/var/log/journal`路径采集journal日志。

* 采集配置1

```yaml
enable: true
inputs:
  - Type: service_journal
    JournalPaths:
      - "/var/log/journal"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出1

```json
{
    "PRIORITY":"6",
    "_GID":"0",
    "_TRANSPORT":"syslog",
    "_SYSTEMD_OWNER_UID":"0",
    "_BOOT_ID":"bab**************54b",
    "_PID":"21848",
    "_MACHINE_ID":"************************",
    "_CAP_EFFECTIVE":"3fffffffff",
    "_COMM":"crond",
    "_HOSTNAME":"iZj*****************1hZ",
    "_SYSTEMD_SLICE":"user-0.slice",
    "MESSAGE":"(root) CMD (/usr/lib64/sa/sa1 1 1)",
    "_CMDLINE":"/usr/sbin/CROND -n",
    "SYSLOG_FACILITY":"9",
    "SYSLOG_IDENTIFIER":"CROND",
    "_AUDIT_LOGINUID":"0",
    "_SYSTEMD_SESSION":"3319",
    "_UID":"0",
    "_EXE":"/usr/sbin/crond",
    "SYSLOG_PID":"21848",
    "_AUDIT_SESSION":"3319",
    "_SYSTEMD_CGROUP":"/user.slice/user-0.slice/session-3319.scope",
    "_SYSTEMD_UNIT":"session-3319.scope",
    "_SOURCE_REALTIME_TIMESTAMP":"1658823001526225",
    "_realtime_timestamp_":"1658823001526482",
    "_monotonic_timestamp_":"1637927744052",
    "__time__":"1658823031"
}
```

### 样例2

Kubernetes场景下，使用Logtail的DaemonSet模式采集宿主机的系统日志，由于日志中有很多并不重要的字段，使用处理插件只挑选较为重要的日志字段。

* 采集配置2

```yaml
enable: true
inputs:
  - Type: service_journal
    ParsePriority: true
    ParseSyslogFacility: true
    JournalPaths:
      - "/logtail_host/var/log/journal"
processors:
  - Type: processor_filter_regex
    Exclude:
      UNIT: "^libcontainer.*test"
  - Type: processor_pick_key 
    Include:
      - MESSAGE
      - PRIORITY
      - _EXE
      - _PID
      - _SYSTEMD_UNIT
      - _realtime_timestamp_
      - _HOSTNAME
      - UNIT
      - SYSLOG_FACILITY
      - SYSLOG_IDENTIFIER
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出2

```json
{
    "MESSAGE":  "ejected connection from \"192.168.0.251:48914\" (error \"EOF\", ServerName "")",
    "PRIORITY":  "informational",
    "SYSLOG_IDENTIFIER":  "etcd",
    "_EXE": "/opt/etcd-v3.3.8/etcd",
    "_HOSTNAME":  "iZb*****************ueZ",
    "_PID":  "10590",
    "_SYSTEMD_UNIT":  "etcd.service",
    "__source__":  "***.***.***.***",
    "__tag__:__hostname__":  "logtail-ds-dp48x", 
    "_realtime_timestamp_":  "1547975837008708",
}
```
