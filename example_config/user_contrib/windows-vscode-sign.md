# Windows.vscode_Microsoft_sign日志

## 提供者

[`SuperGoodGame`](https://github.com/SuperGoodGame)

## 描述

`.vscode_microsoft_sign`记录了用户在windows端启动vscode 中执行过程中的记录，用于在windows系统下登录windows系统，并且申请相关文件使用权限等

默认为打开vscode时直接输出到控制台，如果捕获需要手动操作保存到文件中。

## 日志输入样例

```
2023-07-04 13:20:00.352 [info] Reading sessions from secret storage...
2023-07-04 13:20:00.352 [info] Got 0 stored sessions
2023-07-04 13:20:00.352 [info] Getting sessions for all scopes...
2023-07-04 13:20:00.352 [info] Got 0 sessions for all scopes...
2023-07-04 13:20:00.352 [info] Getting sessions for the following scopes: email offline_access openid profile
2023-07-04 13:20:00.352 [info] Got 0 sessions for scopes: email offline_access openid profile
```

## 日志输出样例

```log
2023-07-04 15:32:41 {"__tag__:__path__":"./simple.log","time":"2023-07-04 13:20:00.352 ","attribute":"info","msg":" Reading sessions from secret storage...","__time__":"1688455960"}
2023-07-04 15:32:41 {"__tag__:__path__":"./simple.log","time":"2023-07-04 13:20:00.352 ","attribute":"info","msg":" Got 0 stored sessions","__time__":"1688455960"}
2023-07-04 15:32:41 {"__tag__:__path__":"./simple.log","time":"2023-07-04 13:20:00.352 ","attribute":"info","msg":" Getting sessions for all scopes...","__time__":"1688455960"}
2023-07-04 15:32:41 {"__tag__:__path__":"./simple.log","time":"2023-07-04 13:20:00.352 ","attribute":"info","msg":" Got 0 sessions for all scopes...","__time__":"1688455960"}
2023-07-04 15:32:41 {"__tag__:__path__":"./simple.log","time":"2023-07-04 13:20:00.352 ","attribute":"info","msg":" Getting sessions for the following scopes: email offline_access openid profile","__time__":"1688455960"}
2023-07-04 15:32:41 {"__tag__:__path__":"./simple.log","time":"2023-07-04 13:20:00.352 ","attribute":"info","msg":" Got 0 sessions for scopes: email offline_access openid profile","__time__":"1688455960"}

```

## 采集配置

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: .
    FilePattern: vscode_Microsoft_sign.log
    MaxDepth: 0
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: ([^[]*)\[([a-z]*)\]([^\n]*)
    Keys:
        - time
        - attribute
        - msg
  
flushers:
  - Type: flusher_sls
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
    Topic: bash_history
  - Type: flusher_stdout
    OnlyStdout: true
```

备注：无
