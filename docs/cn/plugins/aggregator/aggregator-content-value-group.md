# 按Key聚合

## 简介

`aggregator_content_value_group` `aggregator`插件可以实现对单条日志按照指定的 Key 进行聚合。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数             | 类型     | 是否必选 | 说明                                                                                                               |
| ---------------- | -------- | -------- | ------------------------------------------------------------------------------------------------------------------ |
| Type             | String   | 是       | 插件类型，指定为`aggregator_content_value_group`。                                                                               |
| GroupKeys        | []String | 是       | 指定需要按照其值分组的Key列表                                                                                      |
| EnablePackID     | Boolean  | 否       | 是否需要在LogGroup的LogTag中添加__pack_id__字段。如果未添加改参数，则默认在LogGroup的LogTag中添加__pack_id__字段。 |
| Topic            | String   | 否       | LogGroup的Topic名。如果未添加该参数，则默认每个LogGroup的Topic名为空。                                             |
| ErrIfKeyNotFound | Boolean  | 否       | 当指定的Key在Log的Contents中找不到时，是否打印错误日志                                                             |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`reg.log`规则的文件，使用`processor_regex`提取字段后，再按照字段`url`、`method`字段聚合，并将采集结果发送到SLS。

* 输入

```bash
echo '127.0.0.1 - - [10/Aug/2017:14:57:51 +0800] "POST /PutData?Category=YunOsAccountOpLog" 0.024 18204 200 37 "-" "aliyun-sdk-java"' >> /home/test-log/reg.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/reg.log
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: ([\d\.]+) \S+ \S+ \[(\S+) \S+\] \"(\w+) ([^\\"]*)\" ([\d\.]+) (\d+) (\d+) (\d+|-) \"([^\\"]*)\" \"([^\\"]*)\"
    Keys:
      - ip
      - time
      - method
      - url
      - request_time
      - request_length
      - status
      - length
      - ref_url
      - browser
aggregators:
  - Type: aggregator_content_value_group
    GroupKeys:
      - url
      - method
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
