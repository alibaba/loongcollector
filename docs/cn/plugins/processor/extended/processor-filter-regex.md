# 日志过滤

## 简介

`processor_filter_regex processor`插件可以实现对日志的过滤。一条日志只有完全匹配Include中的正则表达式，且不匹配Exclude中的正则表达式时才会被采集，否则直接丢弃。

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数                     | 类型，默认值 | 说明                                                |
| ---------------------- | ------- | ------------------------------------------------- |
| Include                | Map，`{}` |  Key为日志字段，Value为该字段值匹配的正则表达式。Key之间为与关系。如果日志中所有字段的值符合对应的正则表达式，则采集该日志。|
| Exclude                | Map，`{}` | Key为日志字段，Value为该字段值匹配的正则表达式。Key之间为或关系。如果日志中任意一个字段的值符合对应的正则表达式，则不采集该日志。
|

## 样例

采集`/home/test-log/`路径下的`proccessor-filter-regex.log`文件，并按照`Json`格式进行日志解析, 然后对部分日志进行过滤。

* 输入

```bash
echo '{"ip": "10.**.**.**", "method": "POST", "brower": "aliyun-sdk-java"}' >> /home/test-log/proccessor-filter-regex.log
echo '{"ip": "10.**.**.**", "method": "POST", "brower": "chrome"}' >> /home/test-log/proccessor-filter-regex.log
echo '{"ip": "192.168.**.**", "method": "POST", "brower": "aliyun-sls-ilogtail"}' >> /home/test-log/proccessor-filter-regex.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_json
    SourceKey: content
    KeepSource: false
    ExpandDepth: 1
    ExpandConnector: ""
  - Type: processor_filter_regex
    Include:
      ip: "10\\..*"
      method: POST
    Exclude:
      brower: "aliyun.*"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
  "__tag__:__path__": "/home/test-log/proccessor-filter-regex.log",
  "__time__": "1658837955",
  "brower": "chrome",
  "ip": "10.**.**.**",
  "method": "POST"
}
```
