# Golang zap日志

## 提供者

[`jyf111`](https://github.com/jyf111)

## 描述

`zap`是一个高性能的Go语言日志库，默认输出`Json`格式的日志记录，并提供`Debug`，`Info`，`Warn`，`Error`，`Fatal`，`Panic`等日志级别。

使用`processor_json`插件对`Json`格式解析，同时使用`processor_split_log_regex`和`processor_regex`插件来处理触发`Panic`时显示的消息以及多行的调用栈信息。

## 日志输入样例

```json
{"level":"info","msg":"info message","name":"user","email":"user@test.com"}
{"level":"panic","msg":"panic message"}
panic: panic message

goroutine 1 [running]:
go.uber.org/zap/zapcore.CheckWriteAction.OnWrite(0x2, 0x987e770, {0x0, 0x0, 0x0})
```

## 日志输出样例

```json
{
    "__tag__:__path__": "/logs/zap.log",
    "level": "info",
    "msg": "info message",
    "name": "user",
    "email": "user@test.com",
    "__time__": "1689222388"
}

{
    "__tag__:__path__": "/logs/zap.log",
    "panic": "panic message\n\ngoroutine 1 [running]:\ngo.uber.org/zap/zapcore.CheckWriteAction.OnWrite(0x2, 0x987e770, {0x0, 0x0, 0x0})",
    "__time__": "1689222388"
}
```

正确解析了`Json`格式的日志，并且正常采集了非`Json`格式的其它信息。

## 采集配置

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /logs # log directory
    FilePattern: zap.log # log file
processors:
  - Type: processor_split_log_regex
    SplitRegex: \{.+\}
    SplitKey: content
    PreserveOthers: true
  - Type: processor_regex
    SourceKey: content
    Regex: \{"level":"panic".+\}\npanic:\s(.+)
    Keys:
      - panic
    KeepSourceIfParseError: true
  - Type: processor_json
    SourceKey: content
    KeepSource: false
    ExpandDepth: 1
    ExpandConnector: ""
    KeepSourceIfParseError: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
