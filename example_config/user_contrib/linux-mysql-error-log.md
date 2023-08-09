# Linux mysql error.log日志

## 提供者
[`alph00`](https://github.com/alph00)

## 描述

错误日志（Error Log）是 MySQL 中最常用的一种日志，主要记录 MySQL 服务器启动和停止过程中的信息、服务器在运行过程中发生的故障和异常情况等。在 MySQL 数据库中，默认开启错误日志功能。例如：如果 mysqld 检测到某个表需要检查或修复，会写入错误日志。

格式说明:  在error.log中，第一列表示时间戳，第二列表示线程号，第三列表示优先级，第四列表示错误码，第五列表示所在子系统，第六列是错误信息。

下面的配置目的是通过正则表达式将日志字符串中的信息识别并匹配出来，并且将priority为System的日志（主要为系统启停等信息）过滤掉。
从输出结果来看，达到了预期的目的，并且在5条输入日志中只有一条priority为Warning的日志被筛选出来了，其他日志都被过滤掉了。

## 日志输入样例
``` plain
2023-07-08T17:51:01.833256Z 0 [System] [MY-010116] [Server] /usr/sbin/mysqld (mysqld 8.0.33-0ubuntu0.22.04.2) starting as process 1077
2023-07-08T17:51:01.873571Z 1 [System] [MY-013576] [InnoDB] InnoDB initialization has started.
2023-07-08T17:51:02.658251Z 1 [System] [MY-013577] [InnoDB] InnoDB initialization has ended.
2023-07-08T17:51:03.209853Z 0 [Warning] [MY-010068] [Server] CA certificate ca.pem is self signed.
2023-07-08T17:51:03.209889Z 0 [System] [MY-013602] [Server] Channel mysql_main configured to support TLS. Encrypted connections are now supported for this channel.
```

## 日志输出样例
``` json
{
	"__tag__:__path__": "/var/log/mysql/error.log",
	"logged": "2023-07-08T17:51:03.209853",
	"thread_id": "0",
	"priority": "Warning",
	"error_code": "MY-010068",
	"subsystem": "Server",
	"info": "CA certificate ca.pem is self signed.",
	"__time__": "1688860505"
}
```

## 采集配置
``` YAML
enable: true
inputs:
  - Type: file_log
    LogPath: /var/log/mysql/
    FilePattern: error.log 
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: (^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.[0-9]+)Z (\d+) \[(\w+)\] \[(\w+-\d+)\] \[(\w+)\] (.+)
    Keys:
      - logged
      - thread_id
      - priority
      - error_code
      - subsystem
      - info
  - Type: processor_filter_regex
    Exclude:
      priority: System
flushers:
  - Type: flusher_stdout 
    FileName: ./out1.log
```