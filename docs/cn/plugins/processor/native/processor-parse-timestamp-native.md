# 时间解析原生处理插件

## 简介

`processor_parse_timestamp_native`插件解析事件中记录时间的字段，并将结果置为事件的__time__字段。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为processor\_parse\_timestamp\_native。  |
|  SourceKey  |  string  |  是  |  /  |  源字段名。  |
|  SourceFormat  |  string  |  是  |  /  |  日志时间格式。更多信息，请参见表1。  |
|  SourceTimezone  |  string  |  否  |  空  |  日志时间所属时区。格式为GMT+HH:MM（东区）或GMT-HH:MM（西区）。  |

- 表1：时间格式

| **时间格式** | **说明** | **示例** |
| --- | --- | --- |
| %a | 星期的缩写。 | Fri |
| %A | 星期的全称。 | Friday |
| %b | 月份的缩写。 | Jan |
| %B | 月份的全称。 | January |
| %d | 每月第几天，十进制，范围为01~31。 | 07, 31 |
| %h | 月份的缩写，等同于%b。 | Jan |
| %H | 小时，24小时制。 | 22 |
| %I | 小时，12小时制。 | 11 |
| %m | 月份，十进制，范围为01~12。 | 08 |
| %M | 分钟，十进制，范围为00~59。 | 59 |
| %n | 换行符。 | 换行符 |
| %p | AM或PM。 | AM、PM |
| %r | 12小时制的时间组合，等同于%I:%M:%S %p。 | 11:59:59 AM |
| %R | 小时和分钟组合，等同于%H:%M。 | 23:59 |
| %S | 秒数，十进制，范围为00~59。 | 59 |
| %t | Tab符号，制表符。 | 无 |
| %y | 年份，十进制，不带世纪，范围为00~99。 | 04、98 |
| %Y | 年份，十进制。 | 2004、1998 |
| %C | 世纪，十进制，范围为00~99。 | 16 |
| %e | 每月第几天，十进制，范围为1~31。如果是个位数字，前面需要加空格。 | 7、31 |
| %j | 一年中的天数，十进制，范围为001~366。 | 365 |
| %u | 星期几，十进制，范围为1~7，1表示周一。 | 2 |
| %U | 每年的第几周，星期天是一周的开始，范围为00~53。 | 23 |
| %V | 每年的第几周，星期一是一周的开始，范围为01~53。如果一月份刚开始的一周>=4天，则认为是第1周，否则认为下一个星期是第1周。 | 24 |
| %w | 星期几，十进制，范围为0~6，0代表周日。 | 5 |
| %W | 每年的第几周，星期一是一周的开始，范围为00~53。 | 23 |
| %c | 标准的日期和时间。 | Tue Nov 20 14:12:58 2020 |
| %x | 标准的日期，不带时间。 | Tue Nov 20 2020 |
| %X | 标准的时间，不带日期。 | 11:59:59 |
| %s | Unix时间戳。 | 1476187251 |

## 样例

采集文件`/home/test-log/reg.log`，通过正则表达式解析日志内容并提取字段，然后将解析日志中的时间作为事件的__time__，最后将结果输出到stdout。

- 输入

```plain
127.0.0.1 - - [2023-12-28T08:07:12.187340] "POST /PutData?Category=YunOsAccountOpLog" 0.024 18204 200 37 "-" "aliyun-sdk-java"
```

- 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/reg.log
processors:
  - Type: processor_parse_regex_native
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
  - Type: processor_parse_timestamp_native
    SourceKey: time
    SourceFormat: '%Y-%m-%dT%H:%M:%S'
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

- 输出

```json
{
    "__tag__:__path__": "/home/test-log/reg.log",
    "ip": "127.0.0.1",
    "time": "2023-12-28T08:07:12.187340",
    "method": "POST",
    "url": "/PutData?Category=YunOsAccountOpLog",
    "request_time": "0.024",
    "request_length": "18204",
    "status": "200",
    "length": "37",
    "ref_url": "-",
    "browser": "aliyun-sdk-java",
    "__time__": "1703722032"
}
```
