# 字符串替换

## 简介

`processor_string_replace processor`插件可以通过全文、正则匹配、去转义的方式实现文本日志的替换。

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数           | 类型       | 是否必选 | 说明                                                                        |
| ------------ | -------- | ---- | ------------------------------------------------------------------------- |
| Type         | String   | 是    | 插件类型                                                                      |
| SourceKey    | String   | 是    | 匹配字段名                                                       |
| Method         | String | 是  | 无默认值。匹配方式，可选值如下：<br>const：字符串全文替换。<br>regex：使用正则提取替换。<br>unquote：去除转义符。 |
| Match           | String | 否  | 无默认值。匹配指定数据。<br>const：输入需要匹配的字符串。当多个子串符合匹配条件时全部替换。<br>regex：输入需要匹配的正则表达式。当多个子串符合匹配条件时全部替换，也可以用正则分组的方式匹配指定分组。<br>unquote：去除转义符不需要输入。 |
| ReplaceString   | String  | 否    | 默认值""。替换数据。<br>const：为匹配后替换的字符串。<br>regex：为匹配后替换的字符串，支持分组替换。<br>unquote：去除转义符不需要输入。                               |
| DestKey | String  | 否    | 无默认值。字符串替换后的值存储的新字段，默认不存储新字段。                         |

## 样例

### 示例 1：全文匹配与替换

采集`/home/test-log/`路径下的`string_replace.log`文件，测试日志内容的正则匹配与替换功能。

* 输入

```bash
echo 'hello,how old are you? nice to meet you' >> /home/test-log/string_replace.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_string_replace
    SourceKey: content
    Method: const
    Match: 'how old are you?'
    ReplaceString: ''
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/string_replace.log",
    "content": "hello, nice to meet you",
    "__time__": "1680353730"
}
```

### 示例 2：基本正则匹配与替换

采集`/home/test-log/`路径下的`string_replace.log`文件，测试日志内容的正则匹配与替换功能。

* 输入

```bash
echo '2022-09-16 09:03:31.013 \u001b[32mINFO \u001b[0;39m \u001b[34m[TID: N/A]\u001b[0;39m [\u001b[35mThread-30\u001b[0;39m] \u001b[36mc.s.govern.polygonsync.job.BlockTask\u001b[0;39m : 区块采集------结束------\r' >> /home/test-log/string_replace.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_string_replace
    SourceKey: content
    Method: regex
    Match: \\u\w+\[\d{1,3};*\d{1,3}m|N/A
    ReplaceString: ''
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/string_replace.log",
    "content": "2022-09-16 09:03:31.013 INFO  [TID: ] [Thread-30] c.s.govern.polygonsync.job.BlockTask : 区块采集------结束------\r",
    "__time__": "1680353730"
}
```

### 示例 3：根据正则分组匹配与替换并输出到新的字段

采集`/home/test-log/`路径下的`string_replace.log`文件，测试日志内容的正则分组匹配与替换功能。
注：分组替换ReplaceString中不能存在{}，选择分组只能使用$1、$2 这种方式。

* 输入

```bash
echo '10.10.239.16' >> /home/test-log/string_replace.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_string_replace
    SourceKey: content
    Method: regex
    Match: (\d.*\.)\d+
    ReplaceString: $1*/24
    DestKey: new_ip
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/string_replace.log",
    "content": "10.10.239.16",
    "new_ip": "10.10.239.*/24",
    "__time__": "1680353730"
}
```

### 示例 4：替换转义字符

采集`/home/test-log/`路径下的`string_replace.log`文件，测试转义自付替换功能。

* 输入

```bash
echo '{\\x22UNAME\\x22:\\x22\\x22,\\x22GID\\x22:\\x22\\x22,\\x22PAID\\x22:\\x22\\x22,\\x22UUID\\x22:\\x22\\x22,\\x22STARTTIME\\x22:\\x22\\x22,\\x22ENDTIME\\x22:\\x22\\x22,\\x22UID\\x22:\\x222154212790\\x22,\\x22page_num\\x22:1,\\x22page_size\\x22:10}' >> /home/test-log/string_replace.log
echo '\\u554a\\u554a\\u554a' >> /home/test-log/string_replace.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_string_replace
    SourceKey: content
    Method: unquote
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test_log/string_replace.log",
    "content": "{\"UNAME\":\"\",\"GID\":\"\",\"PAID\":\"\",\"UUID\":\"\",\"STARTTIME\":\"\",\"ENDTIME\":\"\",\"UID\":\"2154212790\",\"page_num\":1,\"page_size\":10}",
    "__time__": "1680353730"
}
{
    "__tag__:__path__": "/home/test_log/string_replace.log",
    "content": "啊啊啊",
    "__time__": "1680353730"
}
```
