# Grok

## 简介

`processor_grok`插件可以通过 Grok 语法匹配的模式，实现文本日志的字段提取。[源代码](https://github.com/alibaba/loongcollector/tree/main/plugins/processor/processor_grok.go)

## 版本

[Beta](../../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type                | String，无默认值（必填）                 | 插件类型，固定为`processor_grok` |
| CustomPatternDir    | Array，其中 value 为 String，`[]`      | 自定义的GROK模式所在的文件夹地址，会读取文件夹内的所有文件。需要重启生效。 |
| CustomPatterns      | Map，其中 key 和 value 为 String，`{}` | 自定义的GROK模式，key 为规则名，value 为 grok 表达式。 |
| SourceKey           | String，`content`                     | 需要匹配的目标字段。    |
| Match               | Array，其中 value 为 String，`[]`      | 用来匹配的 Grok 表达式数组。Grok 插件会从上至下依次使用 Match 中的表达式对日志进行匹配，并返回第一个匹配成功的结果。配置多条会影响性能，可以参考[效率与优化](#效率与优化)   |
| TimeoutMilliSeconds | Long，`0`                             | 解析 grok 表达式的最大尝试时间，单位为毫秒，设置为 0 禁用超时。     |
| IgnoreParseFailure  | Boolean，`true`                       | 指定解析失败后的操作，不配置表示放弃解析，直接填充所返回的 content 字段。配置为 false ，表示解析失败时丢弃日志。 |
| KeepSource          | Boolean，`true`                       | 是否保留原字段。     |
| NoKeyError          | Boolean，`false`                      | 无匹配的原始字段时是否报错。     |
| NoMatchError        | Boolean，`true`                      | Match 中的表达式全不匹配时是否报错。     |
| TimeoutError        | Boolean，`true`                      | 匹配超时是否返回错误。      |

## 样例

采集`/home/test-log/`路径下的`processor-grok.log`文件，根据指定的配置选项提取日志信息。

* 采集配置

```yaml
enable: true
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_grok
    SourceKey: content
    KeepSource: false
    CustomPatterns:
      HTTP: '%{IP:client} %{WORD:method} %{URIPATHPARAM:request} %{NUMBER:bytes} %{NUMBER:duration}'
    Match: 
      - '%{HTTP}'
      - '%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}'
      - '%{YEAR:year} %{MONTH:month} %{MONTHDAY:day} %{QUOTEDSTRING:motto}'
    IgnoreParseFailure: false
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输入1

```bash
echo 'begin 123.456 end' >> /home/test-ilogtail/test-log/processor-grok.log
```

* 输出1

```json

{
  "__tag__:__path__":"/home/test-ilogtail/test-log/processor-grok.log",
  "word1":"begin",
  "request_time":"123.456",
  "word2":"end",
  "__time__":"1662618045"
}
```

* 输入2

```bash
echo '2019 June 24 "I am iron man"' >> /home/test-ilogtail/test-log/processor-grok.log
```

* 输出2

```json
{
  "__tag__:__path__":"/home/test-ilogtail/test-log/processor-grok.log",
  "year":"2019",
  "month":"June",
  "day":"24",
  "motto":"\"I am iron man\"",
  "__time__":"1662618059"
}
```

* 输入3

```bash
echo 'WRONG LOG' >> /home/test-ilogtail/test-log/processor-grok.log
```

* 输出3

```json
{
  "__tag__:__path__":"/home/test-ilogtail/test-log/processor-grok.log",
  "__time__":"1662618069"
}
```

* 输入4

```bash
echo '10.0.0.0 GET /index.html 15824 0.043' >> /home/test-ilogtail/test-log/processor-grok.log
```

* 输出4

```json
{
  "__tag__:__path__":"/home/test-ilogtail/test-log/processor-grok.log",
  "client":"10.0.0.0",
  "method":"GET",
  "request":"/index.html",
  "bytes":"15824",
  "duration":"0.043",
  "__time__":"1662618081"
}
```

* 输出说明

在样例一中，processor_grok 插件首先使用 Match 中的第一个表达式 '%{HTTP}' 匹配日志，失败后进行下一个尝试。然后匹配 Match 中的第二个表达式 '%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}' 成功，返回结果。由于 KeepSource 参数设置为 false，所以原日志的 content 的字段被丢弃了。
样例二、样例四与样例一类似，分别使用 Match 中的第三和第一个表达式成功匹配日志。
在样例三中，processor_grok 插件使用 Match 中的三个表达式均匹配失败，所以没有任何结果。又因为 IgnoreParseFailure 设置为了 false，所以匹配失败的样例输出三的 content 字段被丢弃了。

## Grok 模式

`processor_grok` 插件主要使用 SLS 的 [GROK 模式](https://help.aliyun.com/document_detail/129387.html)。SLS数据加工提供了 70+ 常用的 GROK 模式，例如身份证号、邮箱、MAC 地址、IPV4、IPV6、时间解析、URL 解析等。此外，还整合了一些其他通用的 Grok 模式。

所有的 Grok 模式模版请见[链接](https://github.com/alibaba/loongcollector/tree/main/example_config/processor_grok_patterns)。

## 效率与优化

### 正则引擎

由于 Golang 原生的 `regexp` 库不支持一些高级的正则语法，故 `processor_grok` 的正则引擎使用第三方的 [regexp2](https://github.com/dlclark/regexp2) 库。`regexp2` 库与 Golang 原生的 `regexp` 的简单性能对比可以参考源代码中的 [Benchmark 测试](https://github.com/alibaba/loongcollector/tree/main/plugins/processor/processor_grok_benchmark_test.go)。

### 优化匹配失败时间

Grok 语法在匹配失败的情况下时间开销巨大。为了提高插件解析的效率，必须优化匹配失败的时间，这里给出几种可以优化匹配失败时间的思路。

* 表达式与数据尽量完全匹配
* 在 grok 表达式中添加锚点，例如^、$等，减少不必要的匹配尝试
* 设置超时时间，即配置参数中的 TimeoutMilliSeconds 参数

### 多项匹配优化

`processor_grok` 插件支持多项匹配，即可以在 Match 中设置多个 Grok 表达式，依次对日志进行匹配尝试。但使用多项匹配时，由于需要一个个匹配合适的表达式，会经历很多匹配失败的情况。可以参考源代码中的 [Benchmark 测试](https://github.com/alibaba/loongcollector/tree/main/plugins/processor/processor_grok_benchmark_test.go)，其对 Match 中有1、2、3、5条表达式的情况，分别做了简单的模拟匹配失败的性能测试，可以看出效率是成倍降低的。

因此，在使用 `processor_grok` 插件时，最好尽量不使用多项匹配，或者 Match 中设置尽可能少的 Grok 表达式。也可以通过减少重复的匹配，来优化效率。下面是使用分层策略减少重复匹配的一个样例。

* 输入

输入共三条数据

```bash
'8.8.8.8 process-name[666]: a b 1 2 a lot of text at the end'
'8.8.8.8 process-name[667]: a 1 2 3 a lot of text near the end;4'
'8.8.8.8 process-name[421]: a completely different format | 1111'
```

* 常规配置

标准的多项匹配，一个一个匹配完整的表达式。

```yaml

processors:
  - Type: processor_grok
    SourceKey: content
    Match: 
      - '%{IPORHOST:clientip} %{DATA:process_name}\[%{NUMBER:process_id}\]: %{WORD:word_1} %{WORD:word_2} %{NUMBER:number_1} %{NUMBER:number_2} %{DATA:data}'    
      - '%{IPORHOST:clientip} %{DATA:process_name}\[%{NUMBER:process_id}\]: %{WORD:word_1} %{NUMBER:number_1} %{NUMBER:number_2} %{NUMBER:number_3} %{DATA:data};%{NUMBER:number_4}'
      - '%{IPORHOST:clientip} %{DATA:process_name}\[%{NUMBER:process_id}\]: %{DATA:data} \| %{NUMBER:number}'
```

* 优化配置

先统一处理前半部分，然后统一处理后半部分

```yaml
processors:
  - Type: processor_grok
    SourceKey: content
    Match: 
      - '%{IPORHOST:clientip} %{DATA:process_name}\[%{NUMBER:process_id}\]: %{GREEDYDATA:content_2}'
    KeepSource: false
  - Type: processor_grok
    SourceKey: content_2
    Match: 
      - '%{WORD:word_1} %{WORD:word_2} %{NUMBER:number_1} %{NUMBER:number_2} %{GREEDYDATA:data}'
      - '%{WORD:word_1} %{NUMBER:number_1} %{NUMBER:number_2} %{NUMBER:number_3} %{DATA:data};%{NUMBER:number_4}'
      - '%{DATA:data} \| %{NUMBER:number}'
    KeepSource: false
```
