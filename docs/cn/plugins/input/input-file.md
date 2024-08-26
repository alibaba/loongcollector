# 文本日志

## 简介

`input_file`插件可以实现从文本文件中采集日志。采集的日志内容将会保存在事件的`content`字段中。

## 版本

[Stable](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_file。  |
|  FilePaths  |  \[string\]  |  是  |  /  |  待采集的日志文件路径列表（目前仅限1个路径）。路径中支持使用\*和\*\*通配符，其中\*\*通配符仅能出现一次且仅限用于文件名前。  |
|  MaxDirSearchDepth  |  int  |  否  |  0  |  文件路径中\*\*通配符匹配的最大目录深度。仅当日志路径中存在\*\*通配符时有效，取值范围为0～1000。  |
|  ExcludeFilePaths  |  \[string\]  |  否  |  空  |  文件路径黑名单。路径必须为绝对路径，支持使用\*通配符。  |
|  ExcludeFiles  |  \[string\]  |  否  |  空  |  文件名黑名单。支持使用\*通配符。  |
|  ExcludeDirs  |  \[string\]  |  否  |  空  |  目录黑名单。路径必须为绝对路径，支持使用\*通配符  |
|  FileEncoding  |  string  |  否  |  utf8  |  文件编码格式。可选值包括utf8和gbk。  |
|  TailSizeKB  |  uint  |  否  |  1024  |  配置首次生效时，匹配文件的起始采集位置距离文件结尾的大小。如果文件大小小于该值，则从头开始采集，取值范围为0～10485760KB。  |
|  Multiline  |  object  |  否  |  空  |  多行聚合选项。详见表1。  |
|  EnableContainerDiscovery  |  bool  |  否  |  false  |  是否启用容器发现功能。仅当Logtail以Daemonset模式运行，且采集文件路径为容器内路径时有效。  |
|  ContainerFilters  |  object  |  否  |  空  |  容器过滤选项。多个选项之间为“且”的关系，仅当EnableContainerDiscovery取值为true时有效，详见表2。  |
|  ExternalK8sLabelTag  |  map  |  否  |  空  |  对于部署于K8s环境的容器，需要在日志中额外添加的与Pod标签相关的tag。map中的key为Pod标签名，value为对应的tag名。 例如：在map中添加`app: k8s_label_app`，则若pod中包含`app=serviceA`的标签时，会将该信息以tag的形式添加到日志中，即添加字段\_\_tag\_\_:k8s\_label\_app: serviceA；若不包含`app`标签，则会添加空字段\_\_tag\_\_:k8s\_label\_app:  |
|  ExternalEnvTag  |  map  |  否  |  空  |  对于部署于K8s环境的容器，需要在日志中额外添加的与容器环境变量相关的tag。map中的key为环境变量名，value为对应的tag名。 例如：在map中添加`VERSION: env_version`，则当容器中包含环境变量`VERSION=v1.0.0`时，会将该信息以tag的形式添加到日志中，即添加字段\_\_tag\_\_:env\_version: v1.0.0；若不包含`VERSION`环境变量，则会添加空字段\_\_tag\_\_:env\_version:  |
|  AppendingLogPositionMeta  |  bool  |  否  |  false  |  是否在日志中添加该条日志所属文件的元信息，包括\_\_tag\_\_:\_\_inode\_\_字段和\_\_file\_offset\_\_字段。  |
|  FlushTimeoutSecs  |  uint  |  否  |  5  |  当文件超过指定时间未出现新的完整日志时，将当前读取缓存中的内容作为一条日志输出。  |
|  AllowingIncludedByMultiConfigs  |  bool  |  否  |  false  |  是否允许当前配置采集其它配置已匹配的文件。  |

* 表1：多行聚合选项

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Mode  |  string  |  否  |  custom  |  多行聚合模式。可选值包括custom和JSON。  |
|  StartPattern  |  string  |  当Multiline.Mode取值为custom时，至少1个必填  |  空  |  行首正则表达式。  |
|  ContinuePattern  |  string  |  |  空  |  行继续正则表达式。  |
|  EndPattern  |  string  |  |  空  |  行尾正则表达式。  |
|  UnmatchedContentTreatment  |  string  |  否  |  single_line  |  对于无法匹配的日志段的处理方式，可选值如下：<ul><li>discard：丢弃</li><li>single_line：将不匹配日志段的每一行各自存放在一个单独的事件中</li></ul>   |

* 表2：容器过滤选项

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  K8sNamespaceRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在Pod所属的命名空间条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  K8sPodRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在Pod的名称条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  IncludeK8sLabel  |  map  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在pod的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为Pod标签名，value为Pod标签的值，说明如下：<ul><li>如果map中的value为空，则pod标签中包含以key为键的pod都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当pod标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的pod会被匹配；</li><li>其他情况下，当pod标签中存在以key为标签名且以value为标签值的情况时，相应的pod会被匹配。</li></ul></ul>       |
|  ExcludeK8sLabel  |  map  |  否  |  空  |  对于部署于K8s环境的容器，指定需要排除采集容器所在pod的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为pod标签名，value为pod标签的值，说明如下：<ul><li>如果map中的value为空，则pod标签中包含以key为键的pod都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当pod标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的pod会被匹配；</li><li>其他情况下，当pod标签中存在以key为标签名且以value为标签值的情况时，相应的pod会被匹配。</li></ul></ul>       |
|  K8sContainerRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器的名称条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  IncludeEnv  |  map  |  否  |  空  |  指定待采集容器的环境变量条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为环境变量名，value为环境变量的值，说明如下：<ul><li>如果map中的value为空，则容器环境变量中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器环境变量中存在以key为环境变量名且对应环境变量值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器环境变量中存在以key为环境变量名且以value为环境变量值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  ExcludeEnv  |  map  |  否  |  空  |  指定需要排除采集容器的环境变量条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为环境变量名，value为环境变量的值，说明如下：<ul><li>如果map中的value为空，则容器环境变量中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器环境变量中存在以key为环境变量名且对应环境变量值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器环境变量中存在以key为环境变量名且以value为环境变量值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  IncludeContainerLabel  |  map  |  否  |  空  |  指定待采集容器的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则默认为空，表示采集所有容器。支持正则匹配。 map中的key为容器标签名，value为容器标签的值，说明如下：<ul><li>如果map中的value为空，则容器标签中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器标签中存在以key为标签名且以value为标签值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  ExcludeContainerLabel  |  map  |  否  |  空  |  指定需要排除采集容器的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则默认为空，表示采集所有容器。支持正则匹配。 map中的key为容器标签名，value为容器标签的值，说明如下：<ul><li>如果map中的value为空，则容器标签中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器标签中存在以key为标签名且以value为标签值的情况时，相应的容器会被匹配。</li></ul></ul>       |

## 样例

### 采集指定目录下的文件

采集`/home/test-log`路径下的所有文件名匹配`*.log`规则的文件，并将结果输出至stdout。

* 输入

```json
{"key1": 123456, "key2": "abcd"}
```

* 采集配置

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

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/json.log",
    "content": "{\"key1\": 123456, \"key2\": \"abcd\"}",
    "__time__": "1657354763"
}
```

注意：`__tag__` 字段的输出会由于ilogtail版本的不同而存在差别。为了在标准输出中能够准确地观察到 `__tag__`，建议仔细检查以下几点：
- flusher_stdout 的配置中，设置了 `Tags: true`
- 如果使用了较新版本的ilogtail，在观察标准输出时，`__tag__`可能会被拆分为一行单独的信息，先于日志的内容输出（这与文档中的示例输出会有差别），请注意不要观察遗漏。

此注意事项适用于后文所有观察 `__tag__` 字段输出的地方。

### 采集指定目录下的所有文件

采集`/home/test-log`路径下的所有文件名匹配`*.log`规则的文件（含递归），并将结果输出至stdout。

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/**/*.log
    MaxDirSearchDepth: 10
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

### 采集K8s容器文件（仅限iLogtail以Daemonset的方式部署）

采集K8s命名空间`default`中以`deploy`为Pod名前缀、Pod标签包含`version: 1.0`且容器环境变量不为`ID=123`的所有容器中，`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将结果输出至stdout。

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
    EnableContainerDiscovery: true
    ContainerFilters:
      K8sNamespaceRegex: default
      K8sPodRegex: ^(deploy.*)$
      IncludeK8sLabel:
        version: v1.0
      ExcludeEnv:
        ID: 123
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

### 采集简单多行日志

采集文件`/home/test-log/regMulti.log`，文件内容通过行首正则表达式切分日志，然后通过正则表达式解析日志内容并提取字段，并将结果输出到stdout。

* 输入

```plain
[2022-07-07T10:43:27.360266763] [INFO] java.lang.Exception: exception happened
    at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f2(RegexMultiLog.java:108)
    at java.base/java.lang.Thread.run(Thread.java:833)
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/regMulti.log
    Multiline:
      StartPattern: \[\d+-\d+-\w+:\d+:\d+.\d+]\s\[\w+]\s.*
processors:
  - Type: processor_parse_regex_native
    SourceKey: content
    Keys:
      - time
      - level
      - msg
    Regex: \[(\S+)]\s\[(\S+)]\s(.*)
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/regMulti.log",
    "time": "2022-07-07T10:43:27.360266763",
    "level": "INFO",
    "msg": "java.lang.Exception: exception happened
    at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f2(RegexMultiLog.java:108)
    at java.base/java.lang.Thread.run(Thread.java:833)",
    "__time__": "1657161807"
}
```

### 采集复杂多行日志

采集文件`/home/test-log/regMulti.log`，文件内容通过行首和行尾正则表达式切分日志，然后通过正则表达式解析日志内容并提取字段，并将结果输出到stdout。

* 输入

```plain
[2022-07-07T10:43:27.360266763] [ERROR] java.lang.Exception: exception happened
[2022-07-07T10:43:27.360266763]    at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f2(RegexMultiLog.java:108)
[2022-07-07T10:43:27.360266763]    at java.base/java.lang.Thread.run(Thread.java:833)
[2022-07-07T10:43:27.360266763]    ... 23 more
[2022-07-07T10:43:27.360266763] Some user custom log
[2022-07-07T10:43:27.360266763] Some user custom log
[2022-07-07T10:43:27.360266763] [ERROR] java.lang.Exception: exception happened
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/regMulti.log
    Multiline:
      StartPattern: \[\d+-\d+-\w+:\d+:\d+.\d+].*Exception.*
      EndPattern: .*\.\.\. \d+ more
processors:
  - Type: processor_parse_regex_native
    SourceKey: content
    Keys:
      - msg
      - time
    Regex: (\[(\S+)].*)
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/regMulti.log",
    "time": "2022-07-07T10:43:27.360266763",
    "msg": "[2022-07-07T10:43:27.360266763] [ERROR] java.lang.Exception: exception happened\n[2022-07-07T10:43:27.360266763]    at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f2(RegexMultiLog.java:108)\n[2022-07-07T10:43:27.360266763]    at java.base/java.lang.Thread.run(Thread.java:833)\n[2022-07-07T10:43:27.360266763]    ... 23 more"
}
{
    "__tag__:__path__": "/home/test-log/regMulti.log",
    "time": "2022-07-07T10:43:27.360266763",
    "msg": "[2022-07-07T10:43:27.360266763] Some user custom log"
}
{
    "__tag__:__path__": "/home/test-log/regMulti.log",
    "time": "2022-07-07T10:43:27.360266763",
    "msg": "[2022-07-07T10:43:27.360266763] Some user custom log"
}
```
