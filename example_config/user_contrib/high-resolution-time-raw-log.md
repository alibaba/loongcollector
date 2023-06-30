# High Resolution Time Raw Log

## Description

The following configuration parses high resolution time from the input logs and send the original logs with a custom key to the output.

只从日志中提取高精度日志时间，保留原始日志，并对原始日志的字段名重命名后输出。

### Example Input

``` plain
2023-06-30 15:06:14.812 INFO [pool-1-thread-2] HttpServer:27 - Request ID 1 handled by thread 19 at 1657259174812
2023-06-30 15:06:14.813 INFO [pool-1-thread-3] HttpServer:27 - Request ID 2 handled by thread 18 at 1657259174813
2023-06-30 15:06:14.815 INFO [pool-1-thread-5] HttpServer:27 - Request ID 4 handled by thread 21 at 1657259174815
```

### Example Output

``` json
[{
  "__tag__:__path__": "/apps/srv/app.log",
  "messages": "2023-06-30 15:06:14.812 INFO [pool-1-thread-2] HttpServer:27 - Request ID 1 handled by thread 19 at 1657259174812",
  "precise_timestamp": "1688108774812",
  "time": "2023-06-30 15:06:14.812",
  "__time__": "1688108774"
}, {
  "__tag__:__path__": "/apps/srv/app.log",
  "messages": "2023-06-30 15:06:14.813 INFO [pool-1-thread-3] HttpServer:27 - Request ID 2 handled by thread 18 at 1657259174813",
  "precise_timestamp": "1688108774813",
  "time": "2023-06-30 15:06:14.813",
  "__time__": "1688108774"
}, {
  "__tag__:__path__": "/apps/srv/app.log",
  "messages": "2023-06-30 15:06:14.815 INFO [pool-1-thread-5] HttpServer:27 - Request ID 4 handled by thread 21 at 1657259174815",
  "precise_timestamp": "1688108774815",
  "time": "2023-06-30 15:06:14.815",
  "__time__": "1688108774"
}]
```

## Configuration

``` YAML
enable: true
inputs:
  - Type: file_log
    LogPath: /apps/srv
    FilePattern: app.log
processors:
  - Type: processor_regex_accelerate
    SourceKey: content
    LogBeginRegex: '\d{4}-\d{2}-\d{2}.*'
    Regex: '(\d+-\d+-\d+\s\S+)\s[\s\S]*'
    Keys:
      - time
    TimeFormat: '%Y-%m-%d %H:%M:%S'
    AdjustTimezone: true
    LogTimezone: 'GMT+08:00'
    EnablePreciseTimestamp: true
    RawLogTag: messages
flushers:
  - Type: flusher_sls
    Region: cn-xxx
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: xxx_project
    LogstoreName: xxx_logstore
```
