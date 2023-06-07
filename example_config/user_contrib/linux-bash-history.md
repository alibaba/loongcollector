# Linux .bash_history日志

## 提供者
[`messixukejia`](https://github.com/messixukejia)

## 描述
`.bash_history`记录了用户在`Shell`中执行的所有命令，每个用户在自己的主目录下，都有自己的`.bash_history`文件。在用户退出`Shell`时将本次登录输入的所有命令进行保存。

为了适配`iLogtail`文件采集的机制，需要在`/etc/profile.d/bash_history.sh`（以便所有用户生效）中做如下配置：
* 将文件保存时覆盖写切换成追加写。
* 文件超过规定大小前，及时归档，避免原始文件被重写导致重复采集。

```
# 设置追加而不是覆盖
shopt -s histappend  
export HISTTIMEFORMAT="%F %T "

# Set the maximum size of the history file
HISTFILESIZE=1000000

# Backup the history file when it exceeds the maximum size
if [ -f ~/.bash_history ] && [ $(stat -c %s ~/.bash_history) -ge $((HISTFILESIZE*90/100)) ]; then
    mv ~/.bash_history ~/.bash_history.bak
fi
```

## 日志输入样例
```
#1685803902
cat user_log_config.json
#1685803906
cat ~/.bash_history|tail
#1685803907
exit
```

## 日志输出样例
```
{
    "__tag__:__path__": "/root/.bash_history",
    "cmd": "cat user_log_config.json",
    "__time__": "1685803902"
}

{
    "__tag__:__path__": "/root/.bash_history",
    "cmd": "cat ~/.bash_history|tail",
    "__time__": "1685803906"
}

{
    "__tag__:__path__": "/root/.bash_history",
    "cmd": "exit",
    "__time__": "1685803907"
}
```

## 采集配置
```
enable: true
inputs:
  - Type: file_log
    LogPath: /root/
    FilePattern: .bash_history
    MaxDepth: 0
processors:
  - Type: processor_split_log_regex
    SplitRegex: \#\d+
    SplitKey: content
    PreserveOthers: true
  - Type: processor_regex
    SourceKey: content
    Regex: \#(\d+)\n(.*)
    Keys:
        - timestamp
        - cmd
  - Type: processor_strptime
    SourceKey: timestamp
    Format: "%s"
    KeepSource: false
flushers:
  - Type: flusher_sls
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
    Topic: bash_history
  - Type: flusher_stdout
    OnlyStdout: true
```


备注：以上采集配置只会采集`root`用户的操作，如果需要采集其他用户的`.bash_history`，在上述采集配置基础上调整路径即可：
```
inputs:
  - Type: file_log
    LogPath: /homme/
    FilePattern: .bash_history
    MaxDepth: 1
```