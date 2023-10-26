# Json加速

## 简介

`processor_json_accelerate processor`插件以加速模式实现`Json`格式日志的字段提取。

备注：该插件目前仅支持与输入插件file_log配套使用，且不得与其它加速插件混用。

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | String | 是 | 插件类型，指定为`processor_json_accelerate`。 |
| FilterKey | Array | 否 | 用于过滤日志的字段。仅当该字段的值与FilterRegex参数中对应设置的正则表达式匹配时，对应的日志才会被采集。 |
| FilterRegex | Array | 否，当FilterKey参数不为空时必选 | 日志字段过滤的正则表达式。该参数元素个数必须与FilterKey参数的元素个数相同。 |
| TimeKey | String | 否 | 用于解析日志时间格式的字段名称。未配置该字段时，默认使用系统时间作为日志时间。 |
| TimeFormat | String | 否 | 日志时间格式，具体信息参见表1。 |
| AdjustTimezone | Boolean | 否 | 是否调整日志时区。仅在配置了TimeFormat参数后有效。如果未添加该参数，则默认使用false，表示使用机器时区。 |
| LogTimezone | String | 否 | 时区偏移量，格式为GMT+HH:MM（东区）、GMT-HH:MM（西区）。仅当AdjustTimezone参数值为false时有效。 |
| EnableTimestampNanosecond | Boolean | 否 | 是否提取纳秒级时间。如果未添加该参数，则默认使用false，表示不提取纳秒级时间。 |
| EnablePreciseTimestamp | Boolean | 否 | （废弃）是否提取高精度时间。如果未添加该参数，则默认使用false，表示不提取高精度时间。 |
| PreciseTimestampKey | String | 否 | （废弃）保存高精度时间戳的字段。如果未添加该参数，则默认使用precise_timestamp字段。 |
| PreciseTimestampUnit | String | 否 | （废弃）高精度时间戳的单位，取值包括ms（毫秒）、us（微秒）、ns（纳秒）。如果未添加该参数，则默认为ms。 |
| EnableRawLog | Boolean | 否 | 是否上传原始日志。如果未添加该参数，则默认使用false，表示不上传原始日志。|
| RawLogTag | String | 否 | 上传原始日志时，用于存放原始日志的字段，默认值：`__raw__`。 |
| DiscardUnmatch | Boolean | 否 | 是否丢弃匹配失败的日志。如果未添加该参数，则默认使用true，表示丢弃匹配失败的日志。|
| MergeType | String | 否 | 日志聚合方式。可选值包括“topic”和“logstore”。如果未添加该参数，则默认使用topic，表示根据topic聚合。 |
| SensitiveKeys | Map<String, Object> | 否 | 脱敏功能，具体信息参见表2。 |

- 表1:时间格式

| 时间格式 | 说明 | 示例 |
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

- 表2:脱敏信息

| 参数名称 | 数据类型 | 是否必填 | 示例值 | 描述 |
| --- | --- | --- | --- | --- |
| key | String | 是 | content | 日志字段名称。 |
| type | String | 是 | const | 脱敏方式。可选值如下：<br>- const：将敏感内容替换成const字段取值内容。<br>- md5：将敏感内容替换为其对应的MD5值。
| regex_begin | String | 是 | 'password':' | 敏感内容前缀的正则表达式，用于查找敏感内容。使用RE2语法。 |
| regex_content | String | 是 | [^']* | 敏感内容的正则表达式，使用RE2语法。 |
| all | Boolean | 是 | true | 是否替换该字段中所有的敏感内容。可选值如下：<br>- true（推荐）：替换。<br>- false：只替换字段中匹配正则表达式的第一部分内容。 |
| const | String | 否 | "********" | 当type设置为const时，必须配置。 |

## 样例

采集`/home/test-log/`路径下的`json.log`文件。

- 输入

```json
{"url": "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1", "ip": "10.200.98.220", "user-agent": "aliyun-sdk-java", "request": {"status": "200", "latency": "18204"}, "time": "07/Jul/2022:10:30:28"}
```

- 采集配置

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test-log/
    FilePattern: json.log
processors:
  - Type: processor_json_accelerate
flushers:
  - Type: flusher_sls
    Endpoint: cn-xxx.log.aliyuncs.com
    ProjectName: test_project
    LogstoreName: test_logstore
```

- 输出

```json
{
    "__tag__:__path__": "/home/test-log/json.log",
    "url": "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1", 
    "ip": "10.200.98.220", 
    "user-agent": "aliyun-sdk-java", 
    "request": {
        "status": "200", 
        "latency": "18204"
    }, 
    "time": "07/Jul/2022:10:30:28", 
    "__time__": "1657161028"
}
```
