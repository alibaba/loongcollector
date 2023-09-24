# MySQL slow log

## Description

MySQL slow log 记录执行时间较长的SQL查询语句和相关性能指标。分析MySQL slow log可以帮助数据库管理员和开发人员分析和优化数据库性能，通过记录执行时间较长的查询，可以识别潜在的性能瓶颈和优化机会，以便改进查询的执行计划、索引设计和数据库配置，从而提升系统的响应性能和效率。

### Example Input

``` plain
# Time: 2023-08-02T16:48:16.725524Z
# User@Host: db_user[db_user] @ [10.10.10.10] Id: 58839292
# Query_time: 2.598145 Lock_time: 0.000063 Rows_sent: 2000 Rows_examined: 1856543
use crm;
SET timestamp=1690994896;
SELECT * FROM customers WHERE age > 30;
# Time: 2023-08-02T16:48:18.399873Z
# User@Host: db_user[db_user] @ [10.10.10.10] Id: 686752487
# Query_time: 3.642374 Lock_time: 0.000066 Rows_sent: 2000 Rows_examined: 1858544
SET timestamp=1690994898;
SELECT * FROM customers WHERE age > 20;
```

### Example Output

``` json
[{
  "lock_time": "0.000063",
  "query_sql": "use crm;\nSET timestamp=1690994896;\nSELECT * FROM customers WHERE age > 30;",
  "query_time": "2.598145",
  "rows_examined": "1856543",
  "rows_sent": "2000",
  "start_time": "2023-08-02T16:48:16.725524Z",
  "thread_id": "58839292",
  "user": "db_user",
  "user_host": "",
  "user_ip": "10.10.10.10",
  "__path__": "/var/log/mysql/mysql-slow.log",
  "__time__": "1691063371"
}, {
  "lock_time": "0.000066",
  "query_sql": "SET timestamp=1690994898;\nSELECT * FROM customers WHERE age > 20;",
  "query_time": "3.642374",
  "rows_examined": "1858544",
  "rows_sent": "2000",
  "start_time": "2023-08-02T16:48:18.399873Z",
  "thread_id": "686752487",
  "user": "db_user",
  "user_host": "",
  "user_ip": "10.10.10.10",
  "__path__": "/var/log/mysql/mysql-slow.log",
  "__time__": "1691063371"
}]
```

## Configuration

``` YAML
enable: true
inputs:
  - Type: file_log           # 文件输入类型
    LogPath: .               # 文件路径
    FilePattern: mysql_slow.log  # 文件名模式
processors:
  - Type: processor_regex_accelerate
    LogBeginRegex: '# Time:.*'
    Keys:
    - start_time
    - user
    - user_host
    - user_ip
    - thread_id
    - query_time
    - lock_time
    - rows_sent
    - rows_examined
    - query_sql
    Regex: |-
      # Time: (\S+)$
      # User@Host: [\S ]*\[([\S ]+)\] @ ([\S]*)\s*\[([\S ]+)\] Id: (\d+)$
      # Query_time: ([\d.]+)\s+Lock_time: ([\d.]+) Rows_sent: ([\d.]+) Rows_examined: ([\d.]+)$
      ([\S\s]+)
    DiscardUnmatch: false
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
