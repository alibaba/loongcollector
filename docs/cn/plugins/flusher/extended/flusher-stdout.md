# 标准输出/文件

## 简介

`flusher_stdout` `flusher`插件可以实现将采集到的数据，经过处理后，打印到标准输出或者自定义文件。

## 支持的Event类型

| LogGroup(v1) | EventTypeLogging | EventTypeMetric | EventTypeSpan |
| ------------ | ---------------- | --------------- | ------------- |
|      ✅      |      ✅           |       ✅        |      ✅       |

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 发布版本：iLogtail v1.0.27
* 更新记录：
  * iLogtail v1.0.32
    * recover the stdout and stderr when open only stdout in flusher_stdout ([#117](https://github.com/alibaba/loongcollector/pull/117))
  * iLogtail v1.3.0
    * fix: flush buffer before stopping ([#468](https://github.com/alibaba/loongcollector/pull/468))
  * iLogtail v1.4.0
    * feat: Define the metrics data model and modify the plugin manager to support procress the metrics data ([#519](https://github.com/alibaba/loongcollector/pull/519))
    * Define span model and byteArray model ([#571](https://github.com/alibaba/loongcollector/pull/571))
    * feat: add metadata group aggregator supporting group pipelineGroupEvents by metadata keys & enable http flusher and http server input supporting raw protocol  ([#599](https://github.com/alibaba/loongcollector/pull/599))
    * feat: suppprt log event model ([#647](https://github.com/alibaba/loongcollector/pull/647))
  * iLogtail v1.7.1
    * add v2 interface to processor_json ([#984](https://github.com/alibaba/loongcollector/pull/984))
  * iLogtail v2.0.4
    * fix: correct pipeline version recognition issue ([#1427](https://github.com/alibaba/loongcollector/pull/1427))

## 配置参数

| 参数            | 类型      | 是否必选 | 说明                                |
| ------------- | ------- | ---- | --------------------------------- |
| Type          | String  | 是    | 插件类型                              |
| OnlyStdout    | Boolean | 否    | 是否打印打印到标准输出。true表示标准输出；false表示文件。 |
| FileName      | String  | 否    | 打印到文件时，需指定文件名。                    |
| MaxSize       | Int     | 否    | 打印到文件时，需指定单个文件大小。默认1M。            |
| MaxRolls      | Int     | 否    | 打印到文件时，需指定文件的轮转个数。默认为1。           |
| KeyValuePairs | Boolean | 否    |                                   |
| Tags          | Boolean | 否    | 打印 `__tag__`，默认false。如果将flusher-stdout用于调试，建议设置为true。 |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果打印到标准输出。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```
