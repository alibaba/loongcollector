# Linux /var/log/auth.log日志

## 提供者
[`shouao.chen`](https://github.com/shouao)

## 描述

`/var/log/auth.log` 是 `Linux` 系统中记录认证日志的文件，其中主要包含以下几类日志：

- 登录事件日志：记录用户登录系统的事件，包括成功登录和失败登录。

- su 命令日志：记录用户使用 su 命令切换账户的事件。

- sudo 命令日志：记录用户使用 sudo 命令执行特权操作的事件。

- SSH 连接日志：记录 SSH 服务的连接事件，包括成功和失败的连接。

- PAM 日志：记录 PAM（Pluggable Authentication Modules）模块的事件，包括用户认证、账户管理等。

- 认证错误日志：记录用户认证失败的事件，包括密码错误、账户锁定等。

- 认证成功日志：记录用户认证成功的事件，包括密码正确、账户解锁等。

`auth.log`文件对于审计和安全分析非常重要，通过分析`auth.log`可以追踪异常访问，查找入侵行为。`auth.log`默认格式不能采集到年份，为了满足长期可观测性的需要，可以在`/etc/rsyslog.conf`中做如下配置：

找到`$ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat`，将其注释掉，然后加上下面两行

```
$template CustomFormat,"%timegenerated:::date-year%-%timegenerated:::date-month%-%timegenerated:::date-day% %timegenerated:::date-hour%:%timegenerated:::date-minute%:%timegenerated:::date-second% %HOSTNAME% %syslogtag%%msg%\n"
$ActionFileDefaultTemplate CustomFormat
```

之后` sudo systemctl restart rsyslog`重启服务使配置生效后`auth.log`格式更新。

## 日志输入样例

```plain
2023-07-11 23:48:03 localhost sshd[2408098]: Received disconnect from 128.199.185.21 port 54777:11: Bye Bye [preauth]
2023-07-11 23:48:03 localhost sshd[2408098]: Disconnected from authenticating user ubuntu 128.199.185.21 port 54777 [preauth]
2023-07-11 23:48:24 localhost sudo:   shouao : TTY=pts/0 ; PWD=/home/shouao ; USER=root ; COMMAND=/usr/bin/tail /var/log/auth.log
2023-07-11 23:48:24 localhost sudo: pam_unix(sudo:session): session opened for user root by shouao(uid=0)
```

## 日志输出样例

```plain
2023-07-11 15:52:49 {"__tag__:__path__":"./simple.log","time":"2023-07-11 23:48:03","hostname":"localhost","logtag":"sshd[2408098]","msg":"Received disconnect from 128.199.185.21 port 54777:11: Bye Bye [preauth]","__time__":"1689090769"}
2023-07-11 15:52:49 {"__tag__:__path__":"./simple.log","time":"2023-07-11 23:48:03","hostname":"localhost","logtag":"sshd[2408098]","msg":"Disconnected from authenticating user ubuntu 128.199.185.21 port 54777 [preauth]","__time__":"1689090769"}
2023-07-11 15:52:49 {"__tag__:__path__":"./simple.log","time":"2023-07-11 23:48:24","hostname":"localhost","logtag":"sudo","msg":"  shouao : TTY=pts/0 ; PWD=/home/shouao ; USER=root ; COMMAND=/usr/bin/tail /var/log/auth.log","__time__":"1689090769"}
2023-07-11 15:52:49 {"__tag__:__path__":"./simple.log","time":"2023-07-11 23:48:24","hostname":"localhost","logtag":"sudo","msg":"pam_unix(sudo:session): session opened for user root by shouao(uid=0)","__time__":"1689090769"}
```

## 采集配置
```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /var/log/
    FilePattern: auth.log
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: "(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+) (\S+): (.+)"
    Keys:
     - time
     - hostname
     - logtag
     - msg
  - Type: processor_strptime
    SourceKey: time
    Format: "%Y-%m-%d %H:%M:%S"
    KeepSource: false
flushers:
  - Type: flusher_sls
    Region: cn-xxx
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: xxx_project
    LogstoreName: xxx_logstore
```
