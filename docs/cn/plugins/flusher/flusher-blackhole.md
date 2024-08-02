# SLS

## 简介

`flusher_blackhole` `flusher`插件直接丢弃采集的事件，属于原生输出插件，主要用于测试。

## 版本

[Stable](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为flusher\_blackhole。  |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果丢弃。

``` yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_blackhole
```
