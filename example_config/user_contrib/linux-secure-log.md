# 提供者
---
## 提供者

[epicZombie](https://github.com/EpicZombie)
# 描述
---
从secure日志中提取出系统发出的安全题型，并且使用filter去除一些无用信息并留下有效信息输出。

# 日志样例
```shell
Jul  4 12:39:20 VM-4-4-centos sshd[8192]: pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=170.64.134.40
Jul  4 12:39:22 VM-4-4-centos sshd[8192]: Failed password for invalid user sys from 170.64.134.40 port 46144 ssh2
Jul  4 12:39:22 VM-4-4-centos sshd[8192]: Connection closed by 170.64.134.40 port 46144 [preauth]
Jul  4 12:39:28 VM-4-4-centos sshd[8278]: Invalid user bigdata from 170.64.134.40 port 33978
Jul  4 12:39:28 VM-4-4-centos sshd[8278]: input_userauth_request: invalid user bigdata [preauth]
Jul  4 12:39:28 VM-4-4-centos sshd[8278]: pam_unix(sshd:auth): check pass; user unknown
Jul  4 12:39:28 VM-4-4-centos sshd[8278]: pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=170.64.134.40
Jul  4 12:39:30 VM-4-4-centos sshd[8278]: Failed password for invalid user bigdata from 170.64.134.40 port 33978 ssh2
Jul  4 12:39:31 VM-4-4-centos sshd[8278]: Connection closed by 170.64.134.40 port 33978 [preauth]
Jul  4 12:39:37 VM-4-4-centos sshd[8322]: pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=170.64.134.40  user=root
Jul  4 12:39:37 VM-4-4-centos sshd[8322]: pam_succeed_if(sshd:auth): requirement "uid >= 1000" not met by user "root"
Jul  4 12:39:39 VM-4-4-centos sshd[8322]: Failed password for root from 170.64.134.40 port 50044 ssh2
Jul  4 12:39:40 VM-4-4-centos sshd[8322]: Connection closed by 170.64.134.40 port 50044 [preauth]
Jul  4 12:39:45 VM-4-4-centos sshd[8343]: Invalid user ds from 170.64.134.40 port 37878
Jul  4 12:39:45 VM-4-4-centos sshd[8343]: input_userauth_request: invalid user ds [preauth]
Jul  4 12:39:46 VM-4-4-centos sshd[8343]: pam_unix(sshd:auth): check pass; user unknown
Jul  4 12:39:46 VM-4-4-centos sshd[8343]: pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=170.64.134.40
Jul  4 12:39:48 VM-4-4-centos sshd[8343]: Failed password for invalid user ds from 170.64.134.40 port 37878 ssh2
Jul  4 12:39:48 VM-4-4-centos sshd[8343]: Connection closed by 170.64.134.40 port 37878 [preauth]
Jul  4 12:39:54 VM-4-4-centos sshd[8501]: pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=170.64.134.40  user=root
Jul  4 12:39:54 VM-4-4-centos sshd[8501]: pam_succeed_if(sshd:auth): requirement "uid >= 1000" not met by user "root"
```

# 日志输入样例
``` shell
Jul 13 18:55:11 VM-4-4-centos sshd[17022]: pam_unix(sshd:auth): check pass; user unknown
Jul 13 18:55:11 VM-4-4-centos sshd[17022]: pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=74.235.236.64
Jul 13 18:55:13 VM-4-4-centos sshd[17022]: Failed password for invalid user guest from 74.235.236.64 port 25667 ssh2
Jul 13 18:55:14 VM-4-4-centos sshd[17022]: Received disconnect from 74.235.236.64 port 25667:11: Bye Bye [preauth]
Jul 13 18:55:14 VM-4-4-centos sshd[17022]: Disconnected from 74.235.236.64 port 25667 [preauth]
```


# 日志输出样例
```shell
2023-07-13 18:55:35 {"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:11","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"pam_unix(sshd:auth): check pass; user unknown","__time__":"1689245732"}
2023-07-13 18:55:35 {"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:11","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"pam_unix(sshd:auth): authentication failure; logname= uid=0 euid=0 tty=ssh ruser= rhost=74.235.236.64","__time__":"1689245732"}
2023-07-13 18:55:35 {"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:13","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"Failed password for invalid user guest from 74.235.236.64 port 25667 ssh2","__time__":"1689245732"}
2023-07-13 18:55:35 {"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:14","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"Received disconnect from 74.235.236.64 port 25667:11: Bye Bye [preauth]","__time__":"1689245732"}
2023-07-13 18:55:35 {"__tag__:__path__":"/var/log/secure","time":"Jul 13 18:55:14","system":"VM-4-4-centos","disk":"sshd","port":"17022","message":"Disconnected from 74.235.236.64 port 25667 [preauth]","__time__":"1689245732"}
```


# 采集配置
```shell
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