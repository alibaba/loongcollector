# Linux_kern.log日志

## 提供者

[`sunrisefromdark`](https://github.com/sunrisefromdark)

## 描述

`kern.log`日志文件是`Ubuntu`系统中的内核日志文件，它记录了与内核相关的日志消息，路径为`Ubuntu\var\log`。内核是操作系统的核心组件，负责管理系统的资源和提供各种功能。



## 日志输入样例

```
Jul 16 17:24:45 printSomething kernel: [    5.638951] misc dxg: dxgk: dxgkio_query_adapter_info: Ioctl failed: -2

Jul 16 17:24:46 printSomething kernel: [    6.083134] misc dxg: dxgk: dxgkio_query_adapter_info: Ioctl failed: -2

Jul 16 17:25:29 printSomething kernel: [   49.142593] hv_balloon: Max. dynamic memory size: 8118 MB
```



## 日志输出样例

```
2023-07-16 17:25:17 {"__tag__:__path__":"/var/log/kern.log","datetime":"Jul 16 17:24:45","timestamp":"5.638951","message":"misc dxg: dxgk: dxgkio_query_adapter_info: Ioctl failed: -2","__time__":"1689499514"}

2023-07-16 17:25:17 {"__tag__:__path__":"/var/log/kern.log","datetime":"Jul 16 17:24:46","timestamp":"6.083134","message":"misc dxg: dxgk: dxgkio_query_adapter_info: Ioctl failed: -2","__time__":"1689499514"}

2023-07-16 17:25:32 {"__tag__:__path__":"/var/log/kern.log","datetime":"Jul 16 17:25:29","timestamp":"49.142593","message":"hv_balloon: Max. dynamic memory size: 8118 MB","__time__":"1689499529"}
```



## 采集配置

```
enable: true
inputs:
  - Type: file_log
    LogPath: /var/log/
    FilePattern: kern.log
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: "^(\\w+\\s+\\d+\\s\\d+:\\d+:\\d+)\\sprintSomething\\skernel:\\s\\[\\s+(\\d+\\.\\d+)\\]\\s(.+)"
    Keys:
      - datetime
      - timestamp
      - message
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

