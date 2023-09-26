# 文本日志

## 简介

`file_log` `input`插件可以实现从文本文件中采集日志。采集的日志内容将会保存在`content`字段中，后续对该字段进行处理，以实现日志格式的解析。此外，通过`__tag__:__path__`字段也可以查看日志的采集路径。

## 版本

[Stable](../stability-level.md)

## 配置参数

### 基础参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | String | 是 | 插件类型，指定为`file_log`。 |
| LogPath | String | 是 | <p>采集文本日志所在的目录，支持完整目录和通配符两种模式。</p><p>日志文件查找模式为多层目录匹配，即指定目录（包含所有层级的目录）下所有符合条件的文件都会被查找到。 |
| FilePattern | String | 是 | 采集文本日志的文件名，支持完整文件名和通配符两种模式。 |
| MaxDepth | Integer | 否 | 日志目录被监控的最大深度，范围：0~1000。如果未添加该参数，则默认使用0，代表只监控本层目录。 |
| FileEncoding | String | 否 | 日志文件编码格式，取值为utf8、gbk。 默认值为utf8。|
| EnableLogPositionMeta | Boolean | 否 | 是否在日志中添加该日志所属原始文件的元数据信息，即新增__tag__:__inode__字段和__file_offset__字段，默认值为false。 |
| DirBlackList | Array | 否 | 目录（绝对路径）黑名单。支持使用通配符星号（*）匹配多个目录。 |
| FilepathBlackList | Array | 否 | 文件路径（绝对路径）黑名单。支持使用通配符星号（*）匹配多个文件。 |
| ContainerFile | Boolean | 是 | iLogtail与待采集日志是否处于不同环境中。若待采集的日志和iLogtail在不同的容器中，请将参数值置为true，其余情况请置为false。 |
| ContainerInfo | Map<String, Object> | 否 | 容器相关参数，仅当ContainerFile参数为true时有效：<br><ul><li>若您的容器部署于K8s环境中，则可配置表1和表2所示参数来选择待采集容器；<br><li>其它情况下，可配置表2所示参数来选择待采集容器。<p></ul>除此以外，您还可以使用表3所示参数对日志标签进行富化。</p> |
| ReaderFlushTimeout | Boolean | 否 | 发送超时时间（秒），超时会强制读取剩余日志。默认为5秒。|

表1：K8s容器发现相关参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| K8sContainerRegex | String | 否 | 对于部署于K8s环境的容器，指定待采集容器的名称条件，支持正则匹配。如果未添加该参数，则默认为空字符串，表示采集所有容器。 |
| K8sNamespaceRegex | String | 否 | 对于部署于K8s环境的容器，指定待采集容器所在Pod所属的命名空间条件，支持正则匹配。如果未添加该参数，则默认为空字符串，表示采集所有容器。 |
| K8sPodRegex | String | 否 | 对于部署于K8s环境的容器，指定待采集容器所在Pod的名称条件，支持正则匹配。如果未添加该参数，则默认为空字符串，表示采集所有容器。 |
| IncludeK8sLabel | Map<String, String> | 否 | 对于部署于K8s环境的容器，指定待采集容器所在Pod的标签条件，多个条件之间为“或”的关系，即Pod标签满足任一条件即可匹配并被采集：<br><li>如果Map中的Value为空，则Pod标签中包含以Key为键的Pod都会被匹配；<br><li>如果Map中的Value不为空，则<br><ul><li>若Value以<code>^</code>开头并且以<code>$</code>结尾，则当Pod标签中存在以Key为标签名且对应标签值能正则匹配Value的情况时，相应的Pod会被匹配；<br><li>其他情况下，当Pod标签中存在以Key为标签名且以Value为标签值的情况时，相应的Pod会被匹配。<p></ul>如果未添加该参数，则默认为空，表示采集所有容器。</p> |
| ExcludeK8sLabel | Map<String, String> | 否 | 对于部署于K8s环境的容器，指定需要排除采集容器所在Pod的标签条件，多个条件之间为“或”的关系，即Pod标签满足任一条件即可匹配并被排除采集：<br><li>如果Map中的Value为空，则Pod标签中包含以Key为键的Pod都会被匹配；<br><li>如果Map中的Value不为空，则<br><ul><li>若Value以<code>^</code>开头并且以<code>$</code>结尾，则当Pod标签中存在以Key为标签名且对应标签值能正则匹配Value的情况时，相应的Pod会被匹配；<br><li>其他情况下，当Pod标签中存在以Key为标签名且以Value为标签值的情况时，相应的Pod会被匹配。<p></ul>如果未添加该参数，则默认为空，表示采集所有容器。</p> |

表2：普通容器发现相关参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| IncludeLabel | Map<String, String> | 否 | 指定待采集容器的标签条件，多个条件之间为“或”的关系，即容器标签满足任一条件即可匹配并被采集：<br><li>如果Map中的Value为空，则容器标签中包含以Key为键的容器都会被匹配；<br><li>如果Map中的Value不为空，则<br><ul><li>若Value以<code>^</code>开头并且以<code>$</code>结尾，则当容器标签中存在以Key为标签名且对应标签值能正则匹配Value的情况时，相应的容器会被匹配；<br><li>其他情况下，当容器标签中存在以Key为标签名且以Value为标签值的情况时，相应的容器会被匹配。<p></ul>如果未添加该参数，则默认为空，表示采集所有容器。</p> |
| ExcludeLabel | Map<String, String> | 否 | 指定需要排除采集容器的标签条件，多个条件之间为“或”的关系，即容器标签满足任一条件即可匹配并被排除采集：<br><li>如果Map中的Value为空，则容器标签中包含以Key为键的容器都会被匹配；<br><li>如果Map中的Value不为空，则<br><ul><li>若Value以<code>^</code>开头并且以<code>$</code>结尾，则当容器标签中存在以Key为标签名且对应标签值能正则匹配Value的情况时，相应的容器会被匹配；<br><li>其他情况下，当容器标签中存在以Key为标签名且以Value为标签值的情况时，相应的容器会被匹配。<p></ul>如果未添加该参数，则默认为空，表示采集所有容器。</p> |
| IncludeEnv | Map<String, String> | 否 | 指定待采集容器的环境变量条件，多个条件之间为“或”的关系，即容器环境变量满足任一条件即可匹配并被采集：<br><li>如果Map中的Value为空，则容器环境变量中包含以Key为键的容器都会被匹配；<br><li>如果Map中的Value不为空，则<br><ul><li>若Value以<code>^</code>开头并且以<code>$</code>结尾，则当容器环境变量中存在以Key为环境变量名且对应环境变量值能正则匹配Value的情况时，相应的容器会被匹配；<br><li>其他情况下，当容器环境变量中存在以Key为环境变量名且以Value为环境变量值的情况时，相应的容器会被匹配。<p></ul>如果未添加该参数，则默认为空，表示采集所有容器。</p> |
| ExcludeEnv | Map<String, String> | 否 | 指定需要排除采集容器的环境变量条件，多个条件之间为“或”的关系，即容器环境变量满足任一条件即可匹配并被排除采集：<br><li>如果Map中的Value为空，则容器环境变量中包含以Key为键的容器都会被匹配；<br><li>如果Map中的Value不为空，则<br><ul><li>若Value以<code>^</code>开头并且以<code>$</code>结尾，则当容器环境变量中存在以Key为环境变量名且对应环境变量值能正则匹配Value的情况时，相应的容器会被匹配；<br><li>其他情况下，当容器环境变量中存在以Key为环境变量名且以Value为环境变量值的情况时，相应的容器会被匹配。<p></ul>如果未添加该参数，则默认为空，表示采集所有容器。</p> |

表3：日志标签富化相关参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| ExternalK8sLabelTag | Map<String, String> | 否 | <p>对于部署于K8s环境的容器，需要在日志中额外添加的与Pod标签相关的tag。如果未添加该参数，则默认为空，表示不添加额外tag。</p><p>例如：在Map中添加<code>app: k8s_label_app</code>，则若Pod中包含<code>app=serviceA</code>的标签时，会将该信息iLogtail添加到日志中，即添加字段__tag__:k8s_label_app: serviceA；若不包含<code>app</code>标签，则会添加空字段__tag__:k8s_label_app: </p> |
| ExternalEnvTag | Map<String, String> | 否 | <p>需要在日志中额外添加的与容器环境变量相关的tag。如果未添加该参数，则默认为空，表示不添加额外tag。</p><p>例如：在Map中添加<code>VERSION: env_version</code>，则当容器中包含环境变量<code>VERSION=v1.0.0</code>时，会将该信息以tag形式添加到日志中，即添加字段__tag__:env_version: v1.0.0；若不包含<code>VERSION</code>环境变量，则会添加空字段__tag__:env_version: </p> |


### 高级参数

对于所有的日志采集场景，您还可以额外配置如下所示的高级参数：

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| TopicFormat | String | 否 | Topic生成方式。可选值如下：<br>- none：不生成日志主题。<br>- 文件路径正则表达式：将日志文件路径的某一部分作为日志主题。<br>- `customized://自定义主题名`：使用静态自定义主题名。<br>如果未添加该参数，则默认使用none，表示不生成日志主题。 |
| Preserve | Boolean | 否 | 如果一个日志文件在30分钟内没有任何更新，是否继续监 控该文件。如果未添加该参数，则默认使用true，表示始终监控所选文件。 |
| PreserveDepth | Integer | 否 | 最大超时目录深度，仅当Preserve参数值为false时有效。如果未添加该参数，则默认使用1。 |
| ForceMultiConfig | Boolean | 否 | 是否允许该Logtail配置采集其他Logtail配置已匹配的文件。如果未添加该参数，则默认使用false，表示不允许。 |
| TailSizeKB | Integer | 否 | 新文件首次采集的大小，单位为KB。通过首次采集大小，可以确认首次采集的位置，即：<br>- 首次采集时，如果文件小于该值，则从文件内容起始位置开始采集。<br>- 首次采集时，如果文件大于该值，则从距离文件末尾该值的位置开始采集。如果未添加该参数，则默认使用1024。 |
| DelayAlarmBytes | Integer | 否 | 当文件采集位置与当前日志产生位置之间的内容大小超过该值时，产生告警，单位为字节。 如果未添加该参数，则默认使用209715200，即200MB。|
| DelaySkipBytes | Integer | 否 | 当文件采集位置与当前日志产生位置之间的内容大小超过该值时，直接丢弃落后的数据，单位为字节。如果未添加该参数，则默认使用0，表示不丢弃任何数据。 |

## 样例

### 样例1：iLogtail采集主机文件

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件。

* 输入

> echo '{"key1": 123456, "key2": "abcd"}' >> /home/test-log/json.log

* 采集配置

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log/
    FilePattern: "*.log"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/json.log",
    "content": "{\"key1\": 123456, \"key2\": \"abcd\"}",
    "__time__": "1657354763"
}
```

### 样例2：iLogtail以Daemonset的方式采集K8s容器文件

采集K8s命名空间`default`中以`deploy`为Pod名前缀、Pod标签包含`version: 1.0`且容器环境变量不为`ID=123`的所有容器中，`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件。

* 输入

> echo '{"key1": 123456, "key2": "abcd"}' >> /home/test-log/json.log

* 采集配置

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log/
    FilePattern: "*.log"
    ContainerInfo:
      K8sNamespaceRegex: default
      K8sPodRegex: ^(deploy.*)$
      IncludeK8sLabel:
        version: v1.0
      ExcludeEnv:
        ID: 123
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
