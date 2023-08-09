# Windows\Logs\CBS日志

## 提供者

[ChocoFairy](https://github.com/ChocoFairy)

## 描述

`CBS.log`日志文件是在`Windows`下提供有关脱机处理故障的详细信息的处理日志文件，路径为：`C:\Windows\Logs\CBS\CBS.log`，通常用于记录系统的配置信息和事件，可以通过查看文件内容来查看系统的运行情况，如系统Windows update更新日志，windows的错误报告等。 

## 日志输入样例

```
2023-07-02 10:44:52, Info                  CBS    Starting TrustedInstaller finalization.
2023-07-02 10:44:52, Info                  CBS    Lock: Lock removed: WinlogonNotifyLock, level: 8, total lock:6
2023-07-02 10:44:52, Info                  CBS    Ending TrustedInstaller finalization.
```

## 日志输出样例

```json
{
    "__tag__:__path__":"./CBS.log",
    "time":"2023-07-02 10:44:52",
    "log_level":"Info",
    "log_type":"CBS",
    "log_event":"Starting TrustedInstaller finalization.",
    "__time__":"1688572848"
}
{
    "__tag__:__path__":"./CBS.log",
    "time":"2023-07-02 10:44:52",
    "log_level":"Info",
    "log_type":"CBS",
    "log_event":"Lock: Lock removed: WinlogonNotifyLock, level: 8, total lock:6",
    "__time__":"1688572875"
}
{
    "__tag__:__path__":"./CBS.log",
    "time":"2023-07-02 10:44:52",
    "log_level":"Info",
    "log_type":"CBS",
    "log_event":"Ending TrustedInstaller finalization.",
    "__time__":"1688572890"
}

```

## 采集配置

```yaml
enable: true
inputs:
  - Type: file_log           # 文件输入类型
    LogPath: .               # 文件路径
    FilePattern: CBS.log     # 文件名模式
processors:
  - Type: processor_regex
    SourceKey: content
    SplitRegex: (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\s(.*)\s(.*)\s(.*)
    Keys:
       - time
       - log_level
       - log_type
       - log_event
flushers:
  - Type: flusher_stdout     # 标准输出流输出类型
    OnlyStdout: true
```

