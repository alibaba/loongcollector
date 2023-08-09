# Rsync --log-file 日志

## 提供者

[`idawnlight`](https://github.com/idawnlight)

## 描述

Rsync 使用 `--log-file` 参数时，有默认日志格式 `%t [%p] %i %n%L`，各字段含意如下：

```plain
%i an itemized list of what is being updated
%L the string " -> SYMLINK", " => HARDLINK", or "" (where SYMLINK or HARDLINK is a filename)
%n the filename (short form; trailing "/" on dir)
%p the process ID of this rsync session
%t the current date time
```

## 日志输入样例

示例命令：`rsync -avz --progress --delete ./ilogtail ./temp --log-file=/tmp/rsync.log`

```plain
2023/07/11 23:01:36 [89634] receiving file list
2023/07/11 23:01:36 [89634] *deleting output/checkpoint/MANIFEST-000049
2023/07/11 23:01:36 [89634] *deleting output/checkpoint/000048.log
2023/07/11 23:01:36 [89634] .d..t.... output/
2023/07/11 23:01:36 [89634] >f..t.... output/app_info.json
```

## 日志输出样例

```json
{
    "__tag__:__path__": "/tmp/rsync.log",
    "log_time": "2023/07/11 23:01:36",
    "pid": "89634",
    "message": "receiving file list",
    "__time__": "1689087696"
}

{
    "__tag__:__path__": "/tmp/rsync.log",
    "log_time": "2023/07/11 23:01:36",
    "pid": "89634",
    "itemize": "*deleting",
    "path": "output/checkpoint/MANIFEST-000049",
    "__time__": "1689087696"
}

{
    "__tag__:__path__": "/tmp/rsync.log",
    "log_time": "2023/07/11 23:01:36",
    "pid": "89634",
    "itemize": "*deleting",
    "path": "output/checkpoint/000048.log",
    "__time__": "1689087696"
}

{
    "__tag__:__path__": "/tmp/rsync.log",
    "log_time": "2023/07/11 23:01:36",
    "pid": "89634",
    "itemize": ".d..t....",
    "path": "output/",
    "__time__": "1689087696"
}

{
    "__tag__:__path__": "/tmp/rsync.log",
    "log_time": "2023/07/11 23:01:36",
    "pid": "89634",
    "itemize": ">f..t....",
    "path": "output/app_info.json",
    "__time__": "1689087696"
}
```

## 采集配置

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /tmp
    FilePattern: rsync.log
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: '(\S{10} \S{8}) \[(\d+)\] ([<>ch.*]\S{8}) (\S+)(?: (?:->|=>) )?(\S+)?'
    Keys:
      - date
      - time
      - pid
      - itemize
      - path
      - link_target
  - Type: processor_regex
    SourceKey: content
    Regex: (\S{10} \S{8}) \[(\d+)\] (.*)
    Keys:
      - log_time
      - pid
      - message
  - Type: processor_strptime
    SourceKey: "log_time"
    Format: "%Y/%m/%d %H:%M:%S"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
