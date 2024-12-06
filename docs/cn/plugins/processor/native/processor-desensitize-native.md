# 脱敏原生处理插件

## 简介

`processor_filter_regex_native`插件根据事件字段内容来过滤事件。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为processor\_desensitize\_native。  |
|  SourceKey  |  string  |  是  |  /  |  源字段名。  |
|  Method  |  string  |  是  |  /  |  脱敏方式。可选值包括：<ul><li>const：用常量替换敏感内容。</li><li>md5：用敏感内容的MD5值替换相应内容。</li></ul>       |
|  ReplacingString  |  string  |  否，当Method取值为const时必选  |  /  |  用于替换敏感内容的常量字符串。  |
|  ContentPatternBeforeReplacedString  |  string  |  是  |  /  |  敏感内容的前缀正则表达式。  |
|  ReplacedContentPattern  |  string  |  是  |  /  |  敏感内容的正则表达式。  |
|  ReplacingAll  |  bool  |  否  |  true  |  是否替换所有的匹配的敏感内容。  |

## 样例

采集文件`/home/test-log/sen.log`，将日志内容中的密码替换成******，并将结果输出到stdout。

- 输入

```json
{"account":"1812213231432969","password":"04a23f38"}
```

- 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/reg.log
processors:
  - Type: processor_desensitize_native
    SourceKey: content
    Method: const
    ReplacingString: '******'
    ContentPatternBeforeReplacedString: 'password":"'
    ReplacedContentPattern: '[^"]+'
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

- 输出

```json
{
    "__tag__:__path__": "/home/test-log/reg.log",
    "content": "{\"account\":\"1812213231432969\",\"password\":\"******\"}",
    "__time__": "1657161810"
}
```
