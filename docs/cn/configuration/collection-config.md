# 采集配置

在`iLogtail`工作目录里，可以将采集配置存入`user_yaml_config.d`目录下进行数据采集。每个采集配置文件以数据流水线（`data-pipeline`）的形式组织，每个采集配置文件对应一个数据流水线，配置文件为`yaml`格式，文件名以`.yaml`结尾。

一个典型的采集流水线以下部分组成：

* 输入（`Input`）：从数据源采集数据，数据源可以是文本、HTTP数据、Syslog等。
* 处理（`Processor`）：对采集的数据进行解析、过滤、加工等。
* 聚合（`Aggregator`）：`Pipeline`会自带默认聚合插件，一般不需要关注。
* 输出（`Flusher`）：将符合条件的数据发送到指定的存储系统。

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log
    FilePattern: reg.log
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: (\d*-\d*-\d*)\s(.*)
    Keys:
      - time
      - msg
flushers:
  - Type: flusher_sls
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
  - Type: flusher_stdout
    OnlyStdout: true
```

为了降低开发者使用`iLogtail`的门槛，将一些常见的采集配置归档整理到[`example_config`](../../../example_config/)目录.

目前，`iLogtail`支持本地配置文件热加载，即在修改`user_yaml_config.d`中已有的配置或增加新的配置文件后，无需重启iLogtail即可生效。生效最长等待时间默认约为10秒，可通过`config_update_interval`参数进行调整。

**注意：`config_update_interval`参数仅对社区版有效。**