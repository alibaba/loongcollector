# 原始

## 简介
`processor_default`插件不对数据任何操作，只是简单的数据透传。

## 配置参数

`processor_default`插件不需要配置参数


## 样例
采集`/home/test-log/`路径下的`default.log`文件，提取文件的原始数据。

* 输入
```
echo "2022/07/14/11:32:47 test log" >> /home/test-log/default.log
```

* 采集配置
```
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log/
    FilePattern: default.log
processors:
  - Type: processor_default
    SourceKey: content
flushers:
  - Type: flusher_sls
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出
```
{
    "__tag__:__path__":"/home/test-log/default.log",
    "content":"2022/07/14/11:32:47 test log",
    "__time__":"1657769827"}
}
```