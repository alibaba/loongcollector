## 提供者
[`Gump9`](https://github.com/Gump9)

## 描述
`Apache ErrorLog`是记录有关任何错误或异常的信息的地方。`Apache ErrorLog`错误日志中出现的大多数“错误”通常都很轻微。

## 日志输入样例
```
[Thu Feb 14 09:18:47.234230 2012] [core:error] [pid 2333:tid 2347952345] [client ::1:89793] Directory does not exist: /workspaces/ilogtail/bin
[Wes Nov 22 10:48:22.123483 2013] [core:error] [pid 1456:tid 4320345809] [client ::1:32040] File does not exist: /workspaces/ilogtail/.vscode/launch.json
[Thu May 12 08:28:57.652118 2011] [core:error] [pid 8777:tid 4326490112] [client ::1:58619] File does not exist: /usr/local/apache2/htdocs/favicon.ico

```

## 日志输出样例
```
2023-07-13 05:30:44 {"__tag__:__path__":"./log/apache_errorlog.log","timestamp":"Thu Feb 14 09:18:47.234230 2012","day":"Thu","month":"Feb","monthday":"14","time":"09:18:47.234230","year":"2012","data":"error","pid":"2333","tid":"2347952345","client":"::1:89793","message":"Directory does not exist: /workspaces/ilogtail/bin","__time__":"1689226243"}
2023-07-13 05:30:44 {"__tag__:__path__":"./log/apache_errorlog.log","timestamp":"Wed Nov 22 10:48:22.123483 2013","day":"Wed","month":"Nov","monthday":"22","time":"10:48:22.123483","year":"2013","data":"error","pid":"1456","tid":"4320345809","client":"::1:32040","message":"File does not exist: /workspaces/ilogtail/.vscode/launch.json","__time__":"1689226243"}
2023-07-13 05:30:44 {"__tag__:__path__":"./log/apache_errorlog.log","timestamp":"Thu May 12 08:28:57.652118 2011","day":"Thu","month":"May","monthday":"12","time":"08:28:57.652118","year":"2011","data":"error","pid":"8777","tid":"4326490112","client":"::1:58619","message":"File does not exist: /usr/local/apache2/htdocs/favicon.ico","__time__":"1689226243"}

```

## 采集配置
```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: ./log
    FilePattern: apache_errorlog.log
    MaxDepth: 0
processors:
  - Type: processor_grok
    SourceKey: content
    KeepSource: false
    Match:
      - >-
        \[(?<timestamp>%{DAY:day} %{MONTH:month} %{MONTHDAY:monthday}
        %{TIME:time} %{YEAR:year})\] \[core:%{DATA:data}\] \[pid
        %{NUMBER:pid}:tid %{NUMBER:tid}\] \[client %{DATA:client}\]
        %{GREEDYDATA:message}
flushers:
  - Type: flusher_stdout
    FileName: ./output_apache_errorlog.log

```
