# 数据脱敏

## 简介

`processor_desensitize`插件可以通过正则表达式匹配并实现文本日志中敏感数据的脱敏。[源代码](https://github.com/alibaba/loongcollector/tree/main/plugins/processor/processor_desensitize.go)

## 版本

[Beta](../../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type                  | String，无默认值(必填) | 插件类型，固定为`processor_desensitize` |
| SourceKey             | String，无默认值(必填) | 日志字段名称。 |
| Method         | String，无默认值(必填) | 脱敏方式。可选值如下：<br>const：将敏感内容替换成 ReplaceString 参数处配置等字符串。<br>md5：将敏感内容替换为其对应的MD5值。 |
| Match           | String，无默认值(必填) | 指定敏感数据。可选值如下：<br>full：字段全文。<br>regex：使用正则提取敏感数据。 |
| ReplaceString         | String，无默认值      | 用于替换敏感内容等字符串，当 Method 设置为 const 时必选。 |
| RegexBegin            | String，无默认值      | 用于指定敏感内容前缀的正则表达式，当 Match 配置为 regex 时必选。 |
| RegexContent          | String，无默认值      | 用于指定敏感内容的正则表达式，当 Match 配置为 regex 时必选。|

## 样例

采集`/home/test-log/`路径下包含敏感数据的`processor-desensitize.log`文件，根据指定的配置选项提取日志信息。

* 输入
  
```bash
echo "[{'account':'1812213231432969','password':'04a23f38'}, {account':'1812213685634','password':'123a'}]" >> /home/test-ilogtail/test-log/processor-desensitize.log
```

* 采集配置1

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_desensitize
    SourceKey: content
    Method: "const"
    Match: "full"
    ReplaceString: "********"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出1

```json
{
  "content":"********",
}
```

* 采集配置2

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_desensitize
    SourceKey: content
    Method: "const"
    Match: "regex"
    ReplaceString: "********"
    RegexBegin: "'password':'"
    RegexContent: "[^']*"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出2

```json
{
  "content":"[{'account':'1812213231432969','password':'********'}, {'account':'1812213685634','password':'********'}]",
}
```

* 采集配置3

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_desensitize
    SourceKey: content
    Method: "md5"
    Match: "regex"
    ReplaceString: "********"
    RegexBegin: "'password':'"
    RegexContent: "[^']*"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出3

```json
{
  "content":"[{'account':'1812213231432969','password':'9c525f463ba1c89d6badcd78b2b7bd79'}, {'account':'1812213685634','password':'1552c03e78d38d5005d4ce7b8018addf'}]",
}
```
