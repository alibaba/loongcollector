# com.west2online.ClashX.log

## 提供者

[liuw515](https://github.com/liuw515)

## 描述

`.com.west2online.ClashX.log`记录了使用ClashX软件进行网络节点链接时的网络访问日志，方便记录当前机器使用的TCP IP地址以及端口，另外也记录当前使用节点及访问域名。储存位置在`/Users/UserName/Library/Logs/ClashX`

## 日志输入样例

```shell
2023/07/13 12:33:52.881  [info] [TCP] 127.0.0.1:64873 --> adservice.google.com:443 match DomainKeyword(google) using 节点选择[【I】日本 VIP 3 - chatGPT]
2023/07/13 12:32:33.810  [info] [TCP] 127.0.0.1:64711 --> github.githubassets.com:443 match DomainKeyword(github) using 节点选择[【A】香港  VIP 1 - Nearoute]
2023/07/13 12:30:56.540  [info] [TCP] 127.0.0.1:64660 --> self.events.data.microsoft.com:443 match Match() using 国外网站[【L】新加坡  VIP4 - chatGPT]
2023/07/13 12:28:53.232  [info] [TCP] 127.0.0.1:64591 --> h-adashx.dingtalkapps.com:443 match GeoIP(CN) using DIRECT
```

## 日志输出样例

```shell
{
    "__tag__:__path__":"./Logs/ClashX/com.west2online.ClashX.log",
    "event_time":"2023/07/13 12:33:52.881",
    "ip_address":"127.0.0.1",
    "port":"64873",
    "domain_accessed":"adservice.google.com",
    "method":"DomainKeyword(google)",
    "internet_node":"节点选择[【I】日本 VIP 3 - chatGPT]",
    "__time__":"1689223070"
}
{
    "__tag__:__path__":"./Logs/ClashX/com.west2online.ClashX.log",
    "event_time":"2023/07/13 12:32:33.810",
    "ip_address":"127.0.0.1",
    "port":"64711",
    "domain_accessed":"github.githubassets.com",
    "method":"DomainKeyword(github)",
    "internet_node":"节点选择[【A】香港  VIP 1 - Nearoute]",
    "__time__":"1689223070"
}
{
    "__tag__:__path__":"./Logs/ClashX/com.west2online.ClashX.log",
    "event_time":"2023/07/13 12:30:56.540",
    "ip_address":"127.0.0.1",
    "port":"64660",
    "domain_accessed":"self.events.data.microsoft.com",
    "method":"Match()",
    "internet_node":"国外网站[【L】新加坡  VIP4 - chatGPT]",
    "__time__":"1689223070"
}
{
    "__tag__:__path__":"./Logs/ClashX/com.west2online.ClashX.log",
    "event_time":"2023/07/13 12:28:53.232",
    "ip_address":"127.0.0.1",
    "port":"64591",
    "domain_accessed":"h-adashx.dingtalkapps.com",
    "method":"GeoIP(CN)",
    "internet_node":"DIRECT",
    "__time__":"1689223070"
}
```

## 采集配置

```shell
enable: true
inputs:
  - Type: file_log          
    LogPath: ./Logs/ClashX
    FilePattern: com.west2online.ClashX.log

processors:
  - Type: processor_regex
    SourceKey: content
    Regex: '^(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}\.\d{3}) \s+\[info\] \[TCP\] (\d+\.\d+\.\d+\.\d+):(\d+) --> ([^\s]+):\d+ match ([^\s]+) using (.*)$'
    Keys:
      - event_time
      - ip_address
      - port
      - domain_accessed
      - method
      - internet_node
    KeepSource: false
    KeepSourceIfParseError: true
    NoKeyError: false
    NoMatchError: true

flushers:
  - Type: flusher_sls
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
  - Type: flusher_stdout
    OnlyStdout: true
```
