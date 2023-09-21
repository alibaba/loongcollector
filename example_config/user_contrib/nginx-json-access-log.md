# Nginx JSON Access log

## Description

Nginx可以将access log配置为JSON格式以打印结构化日志。结构化日志会将内层嵌套的JSON序列化为JSON字符串，可以通过下列配置进行展开。

``` nginx
log_format user_log_format escape=json '{"@timestamp":"$time_iso8601",'
                      '"server_addr":"$server_addr",'
                      '"remote_addr":"$remote_addr",'
                      '"scheme":"$scheme",'
                      '"request_method":"$request_method",'
                      '"request_uri": "$request_uri",'
                      '"request_length": "$request_length",'
                      '"uri": "$uri", '
                      '"request_time":$request_time,'
                      '"body_bytes_sent":$body_bytes_sent,'
                      '"bytes_sent":$bytes_sent,'
                      '"status":"$status",'
                      '"upstream_time":"$upstream_response_time",'
                      '"upstream_host":"$upstream_addr",'
                      '"upstream_status":"$upstream_status",'
                      '"host":"$host",'
                      '"http_referer":"$http_referer",'
                      '"http_user_agent":"$http_user_agent",'
                      '"data": "$data"'
                      '}';
```

### Example Input

``` json
{"@timestamp":"2023-08-24T21:31:52+08:00","server_addr":"172.0.0.10","remote_addr":"111.111.111.111","scheme":"https","request_method":"POST","request_uri": "/x.gif","request_length": "1024","uri": "/x.gif", "request_time":0.003,"body_bytes_sent":11,"bytes_sent":443,"status":"200","upstream_time":"","upstream_host":"","upstream_status":"","host":"api.lemonttt.com","http_referer":"https://xxx.com/","http_user_agent":"Mozilla/5.0 (Linux; U; Android 12; zh-cn) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/100.0.4896.58","data": "{\"url\":\"https://xxx.com/\",\"token\":\"xxx\",\"font_list\":[\"Calibri\",\"Century\",\"Century Gothic\"],\"contrast_preference\":\"no-preference\",\"colors_inverted\":null}"}
```

### Example Output

``` json
[{
  "__path__": "./access.log",
  "@timestamp": "2023-08-24T21:31:52+08:00",
  "server_addr": "172.0.0.10",
  "remote_addr": "111.111.111.111",
  "scheme": "https",
  "request_method": "POST",
  "request_uri": "/x.gif",
  "request_length": "1024",
  "uri": "/x.gif",
  "request_time": "0.003",
  "body_bytes_sent": "11",
  "bytes_sent": "443",
  "status": "200",
  "upstream_time": "",
  "upstream_host": "",
  "upstream_status": "",
  "host": "api.lemonttt.com",
  "http_referer": "https://xxx.com/",
  "http_user_agent": "Mozilla/5.0 (Linux; U; Android 12; zh-cn) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/100.0.4896.58",
  "data.url": "https://xxx.com/",
  "data.token": "xxx",
  "data.font_list": "[\"Calibri\",\"Century\",\"Century Gothic\"]",
  "data.contrast_preference": "no-preference",
  "data.colors_inverted": "null",
  "__time__": "1692883912"
}]
```

## Configuration

``` YAML
enable: true
inputs:
  - Type: file_log           # 文件输入类型
    LogPath: .               # 文件路径
    FilePattern: access.log  # 文件名模式
processors:
  - Type: processor_json
    SourceKey: content
    KeepSource: false
    ExpandDepth: 1
    ExpandConnector: ""
  - Type: processor_json
    SourceKey: data
    KeepSource: false
    UseSourceKeyAsPrefix: true
    ExpandDepth: 1
    ExpandConnector: "."
  - Type: processor_strptime
    SourceKey: "@timestamp"
    Format: "%Y-%m-%dT%H:%M:%S+08:00"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
