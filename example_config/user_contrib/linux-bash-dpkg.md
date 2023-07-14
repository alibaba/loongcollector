# Linux dpkg.log日志

## 提供者

[Lr-Young]([Lr-Young (github.com)](https://github.com/Lr-Young))

## 描述

​	`dpkg` 即 package manager for Debian ，是 Debian 和基于 Debian 的系统中一个主要的**包管理工具**，可以用来安装、构建、卸载、管理 `deb` 格式的软件包。

​	dpkg.log日志记录linux用户使用dpkg命令安装、管理linux软件包的过程，每一条日志记录包括dpkg操作时间、dpkg操作信息、软件包名称、软件包版本、附加信息

## 日志输入样例

```
2023-07-08 10:44:34 configure git:amd64 1:2.25.1-1ubuntu3.11 <无>
2023-07-08 10:44:34 status unpacked git:amd64 1:2.25.1-1ubuntu3.11
2023-07-08 10:44:34 status half-configured git:amd64 1:2.25.1-1ubuntu3.11
2023-07-08 10:44:34 status installed git:amd64 1:2.25.1-1ubuntu3.11
```

## 日志输出样例

```json
{"__tag__:__path__":"./dpkg.log","date":"2023-07-08","time":"10:44:34","action":"configure","package":"git:amd64","version":"1:2.25.1-1ubuntu3.11","appendix":" <无>","__time__":"1688910976"}
{"__tag__:__path__":"./dpkg.log","date":"2023-07-08","time":"10:44:34","action":"status unpacked","package":"git:amd64","version":"1:2.25.1-1ubuntu3.11","appendix":"","__time__":"1688910976"}
{"__tag__:__path__":"./dpkg.log","date":"2023-07-08","time":"10:44:34","action":"status","package":"half-configured","version":"git:amd64","appendix":" 1:2.25.1-1ubuntu3.11","__time__":"1688910976"}
{"__tag__:__path__":"./dpkg.log","date":"2023-07-08","time":"10:44:34","action":"status installed","package":"git:amd64","version":"1:2.25.1-1ubuntu3.11","appendix":"","__time__":"1688910976"}

```

## 采集配置

```yaml
enable: true
inputs:
  - Type: file_log         
    LogPath: /var/log/             
    FilePattern: dpkg.log 
processors: 
  - Type: processor_regex
    SourceKey: content
    Regex: (\d{4}-\d{2}-\d{2})\s(\d{2}:\d{2}:\d{2})\s(status\s\w+|\w+)\s(\S+)\s(\S+)(\s.*|)
    Keys: 
      - date
      - time
      - action
      - package
      - version
      - appendix
flushers:
  - Type: flusher_stdout   
    OnlyStdout: true
```
