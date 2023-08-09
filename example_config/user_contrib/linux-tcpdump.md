# Linux tcpdump数据包信息

## 提供者

[`jyf111`](https://github.com/jyf111)

## 描述

`tcpdump`是一个Linux上的抓包工具。`-i`选项可以指定监听的网卡接口，`tcp`选项指定监听`tcp`报文。

在此基础上，希望通过`ilogtail`实时观测`tcpdump`的输出（重定向到文件中），并且只采集出`syn`包和`fin`包。

`tcpdump`中`tcp`报文的输出格式如下：（见`man tcpdump`）

```plain
src > dst: Flags [tcpflags], seq data-seqno, ack ackno, win window, urg urgent, options [opts], length len
```

`syn`包和`fin`包在tcpflags中分别包含S和F标记。

使用`processor_filter_regex`插件实现日志过滤，仅采集与`Include`参数对应的正则表达式匹配的字段值。

## 日志输入样例

```plain
13:45:51.894506 IP 192.168.111.111.11111 > 110.242.68.66.http: Flags [S], seq 3449331597, win 64240, options [mss 1460,sackOK,TS val 2834610279 ecr 0,nop,wscale 7], length 0
13:45:51.909884 IP 110.242.68.66.http > 192.168.111.111.11111: Flags [S.], seq 2926209326, ack 3449331598, win 8192, options [mss 1452,sackOK,nop,nop,nop,nop,nop,nop,nop,nop,nop,nop,nop,wscale 5], length 0
13:45:51.909964 IP 192.168.111.111.11111 > 110.242.68.66.http: Flags [.], ack 1, win 502, length 0
13:45:51.967957 IP 110.242.68.66.http > 192.168.111.111.11111: Flags [.], ack 126, win 772, length 0
13:45:51.967958 IP 110.242.68.66.http > 192.168.111.111.11111: Flags [F.], seq 387, ack 126, win 772, length 0
13:45:51.968018 IP 192.168.111.111.11111 > 110.242.68.66.http: Flags [.], ack 388, win 501, length 0
```

## 日志输出样例

```json
{
    "__tag__:__path__": "/logs/dump.log",
    "content": "13:45:51.894506 IP 192.168.111.111.11111 > 110.242.68.66.http: Flags [S], seq 3449331597, win 64240, options [mss 1460,sackOK,TS val 2834610279 ecr 0,nop,wscale 7], length 0",
    "__time__": "1688795555"
}

{
    "__tag__:__path__": "/logs/dump.log",
    "content": "13:45:51.909884 IP 110.242.68.66.http > 192.168.111.111.11111: Flags [S.], seq 2926209326, ack 3449331598, win 8192, options [mss 1452,sackOK,nop,nop,nop,nop,nop,nop,nop,nop,nop,nop,nop,wscale 5], length 0",
    "__time__": "1688795555"
}

{
    "__tag__:__path__": "/logs/dump.log",
    "content": "13:45:51.967958 IP 110.242.68.66.http > 192.168.111.111.11111: Flags [F.], seq 387, ack 126, win 772, length 0",
    "__time__": "1688795555"
}
```

只采集了带有S或F标记的数据报文。

## 采集配置

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /logs # log directory
    FilePattern: dump.log # log file
processors:
  - Type: processor_filter_regex
    Include:
      content: .+ Flags \[[SF]\.?\].+
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
