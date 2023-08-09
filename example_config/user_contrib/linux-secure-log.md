## 提供者
---
[epicZombie](https://github.com/EpicZombie)

## 描述
---
从secure日志中提取出系统发出的安全提醒，并进行格式化输出。

## 日志输入样例
``` shell
Jul 13 18:55:11 VM-4-4-centos sshd[17022]: pam_unix(sshd:auth): check pass; user unknown
Jul 13 18:55:11 VM-4-4-centos sshd[17022]: pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=74.235.236.64
Jul 13 18:55:13 VM-4-4-centos sshd[17022]: Failed password for invalid user guest from 74.235.236.64 port 25667 ssh2
Jul 13 18:55:14 VM-4-4-centos sshd[17022]: Received disconnect from 74.235.236.64 port 25667:11: Bye Bye [preauth]
Jul 13 18:55:14 VM-4-4-centos sshd[17022]: Disconnected from 74.235.236.64 port 25667 [preauth]
```

## 日志输出样例
```json
{"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:11","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"pam_unix(sshd:auth): check pass; user unknown","__time__":"1689245732"}
{"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:11","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=74.235.236.64","__time__":"1689245732"}
{"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:13","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"Failed password for invalid user guest from 74.235.236.64 port 25667 ssh2","__time__":"1689245732"}
{"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:14","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"Received disconnect from 74.235.236.64 port 25667:11: Bye Bye [preauth]","__time__":"1689245732"}
{"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:14","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"Disconnected from 74.235.236.64 port 25667 [preauth]","__time__":"1689245732"}
```

## 采集配置
```yaml
enable: true
inputs:
  - Type: file_log          # 文件输入类型
    LogPath: /var/log/              # 文件路径Glob匹配规则
    FilePattern: secure # 文件名Glob匹配规则
    maxDepth:0
processors:
  - Type: processor_filter_regex
    Include:
      content: '(\w+-\d+-\d+-\w+\s\w+)'
  - Type: processor_regex
    SourceKey: content
    Regex: '^(\w+\s+\d+\s+\d+:\d+:\d+)\s+([\w-]+)\s+(\w+)\[(\d+)\]:\s+(.*)'
    Keys: ["time", "system", "disk", "port", "message"]
flushers:
  - Type: flusher_stdout    # 标准输出流输出类型
    OnlyStdout: ./secure-log.out
```