# Redis.monitor日志

## 提供者

[`YangZhang-dev`](https://github.com/YangZhang-dev)

## 描述

使用`redis-cli monitor`可以监控redis正在执行的命令，将输出重定向至文件中方便ilogtail采集：`nohup redis-cli monitor > ./redis.log &`

## 日志输入样例

```log
1688222331.238859 [11 192.168.72.1:60531] "ping"
1688222336.739487 [11 192.168.72.1:60531] "lpush" "test_list" "New member"
1688222336.750862 [11 192.168.72.1:60531] "type" "test_list"
1688222336.773591 [11 192.168.72.1:60531] "ttl" "test_list"
1688222336.779322 [11 192.168.72.1:60531] "lrange" "test_list" "0" "199"
```

下面将其采集为五个字段，分别是timestamp，db，ip，port，command

## 日志输出样例

```log
2023-07-01 22:38:53 {"__tag__:__path__":"./redis.log","timestamp":"1688222331.238859","db":"11","ip":"192.168.72.1","port":"60531","command":" \"ping\"","__time__":"1688222331"}
2023-07-01 22:38:59 {"__tag__:__path__":"./redis.log","timestamp":"1688222336.739487","db":"11","ip":"192.168.72.1","port":"60531","command":" \"lpush\" \"test_list\" \"New member\"","__time__":"1688222336"}
2023-07-01 22:38:59 {"__tag__:__path__":"./redis.log","timestamp":"1688222336.750862","db":"11","ip":"192.168.72.1","port":"60531","command":" \"type\" \"test_list\"","__time__":"1688222336"}
2023-07-01 22:38:59 {"__tag__:__path__":"./redis.log","timestamp":"1688222336.773591","db":"11","ip":"192.168.72.1","port":"60531","command":" \"ttl\" \"test_list\"","__time__":"1688222336"}
2023-07-01 22:38:59 {"__tag__:__path__":"./redis.log","timestamp":"1688222336.779322","db":"11","ip":"192.168.72.1","port":"60531","command":" \"lrange\" \"test_list\" \"0\" \"199\"","__time__":"1688222336"}
```

## 采集配置

```yaml
enable: true
inputs:
  - Type: file_log                    # 文件输入类型
    LogPath: ./                       # 文件路径
    FilePattern: redis.log            # 文件名模式
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: ([\d+\.]+) \[(\d{1,2}) ([\d\.]+)\:(\d+)\]( \".+\")+
    Keys:
      - timestamp
      - db
      - ip
      - port
      - command
flushers:
  - Type: flusher_stdout              # 标准输出流输出类型
    OnlyStdout: true
```
