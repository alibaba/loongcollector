# Django Application 服务日志

## 提供者

[mattquminzhi](https://github.com/quminzhi)

## 描述

Django Application服务日志包含系统Debug日志，开发者自定义Debug日志，RESTFUL访问日志等。本样例主要展示如何使用ilogtail配置采集开发者自定义Debug日志。

```shell
System check identified no issues (0 silenced).
July 11, 2023 - 21:37:51
Django version 4.2.3, using settings 'server.settings'
Starting development server at http://127.0.0.1:8000/
Quit the server with CONTROL-C.

# 开发者自定义Debug日志
[11/Jul/2023 21:37:50] [INFO] Starting Devzone application ...
[11/Jul/2023 21:37:52] [INFO] Application started
...
[11/Jul/2023 21:38:57] [WARNING] CPU workload is heavy and working threads are reduced to 8
...
[11/Jul/2023 21:39:55] [INFO] Ending Devzone application
```

## 日志输入样例

```shell
[11/Jul/2023 21:37:50] [INFO] Starting Devzone application ...
[11/Jul/2023 21:37:52] [INFO] Application started
[11/Jul/2023 21:37:54] "GET / HTTP/1.1" 200 19741
[11/Jul/2023 21:37:55] "GET /static/css/style.css HTTP/1.1" 200 19366
[11/Jul/2023 21:37:55] "GET /static/js/script.js HTTP/1.1" 200 3057
[11/Jul/2023 21:37:55] "GET /static/images/W-Logo_RegistrationMark_Tan_7504.png HTTP/1.1" 200 4321
[11/Jul/2023 21:37:55] "GET /static/images/avatar.svg HTTP/1.1" 200 1826
[11/Jul/2023 21:37:55] "GET /images/users/Screen_Shot_2021-11-21_at_8.33.32_AM.png HTTP/1.1" 200 259330
[11/Jul/2023 21:37:55] "GET /images/users/Screen_Shot_2021-11-21_at_8.39.07_AM.png HTTP/1.1" 200 281226
[11/Jul/2023 21:37:55] "GET /images/users/favicon_8YPMSCm.png HTTP/1.1" 200 406007
[11/Jul/2023 21:38:57] [WARNING] CPU workload is heavy and working threads are reduced to 8
[11/Jul/2023 21:39:20] "GET /static/images/avatar.svg HTTP/1.1" 200 1826
[11/Jul/2023 21:39:55] [INFO] Ending Devzone application
```

## 日志输出样例

```shell
# Django Debug Out
2023-07-11 22:57:26 {"__tag__:__path__":"./logs/django.log.20230711","time":"11/Jul/2023 21:37:50","level":"INFO","message":"Starting Devzone application ...","__time__":"1689116243"}
2023-07-11 22:57:26 {"__tag__:__path__":"./logs/django.log.20230711","time":"11/Jul/2023 21:37:52","level":"INFO","message":"Application started","__time__":"1689116243"}
2023-07-11 22:57:26 {"__tag__:__path__":"./logs/django.log.20230711","time":"11/Jul/2023 21:38:57","level":"WARNING","message":"CPU workload is heavy and working threads are reduced to 8","__time__":"1689116243"}
```

## 采集配置

```yaml
enable: true 
inputs:
  - Type: file_log
    LogPath: ./logs
    FilePattern: django.log.*
    MaxDepth: 0
processors:
  - Type: processor_filter_regex
    Include:
      content: '^\[\d+.*\]\s\[\w+\].*$'
  - Type: processor_regex
    SourceKey: content
    Regex: '^\[(\d+.*)\]\s\[(INFO|WARNING|ERRO)\]\s(.*)$'
    Keys: ["time", "level", "message"]
flushers:
  - Type: flusher_stdout
    FileName: ./django-debug-log.out
```
