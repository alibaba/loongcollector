日志时间提取（Golang）
---

该插件可从指定字段中提取日志时间，并将其转换为其他字段，提取及转换所使用的时间格式为 [Golang Time Format](https://golang.org/pkg/time/#Time.Format)。

#### 参数说明

插件类型（type）为 `processor_gotime`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|SourceKey|string|必选|源Key，为空不生效。|
|SourceFormat|string|必选|源key解析的go时间格式。|
|SourceLocation|int|可选|源时区，为空表示本机时区。|
|DestKey|string|必选|目标Key，为空不生效。|
|DestFormat|string|必选|目标key解析的go时间格式。|
|DestLocation|int|可选|目标时区，为空表示本机时区。|
|SetTime|bool|可选|是否设置到时间字段，默认为true。|
|KeepSource|bool|可选|是否保留源字段，默认为true。|
|NoKeyError|bool|可选|无匹配的key是否记录，默认为true。|
|AlarmIfFail|bool|可选|失败是否记录，默认为true。|

#### 示例1
以格式 `2006-01-02 15:04:05`（东八区）解析字段 `s_key` 的值作为日志时间，并以格式 `2006/01/02 15:04:05`（东九区）将日志时间转换到新字段 `d_key` 中。

配置详情及处理结果如下：

- 输入

```
"s_key":"2019-07-05 19:28:01"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_gotime",
      "detail": {
        "SourceKey": "s_key",
        "SourceFormat":"2006-01-02 15:04:05",
        "SourceLocation":8,
        "DestKey":"d_key",
        "DestFormat":"2006/01/02 15:04:05",
        "DestLocation":9,
        "SetTime": true,
        "KeepSource": true,
        "NoKeyError": true,
        "AlarmIfFail": true
      }
    }
  ]
}
```

- 配置后结果

```
"s_key":"2019-07-05 19:28:01"
"d_key":"2019/07/05 20:28:01"
```

#### 示例2
以 Unix 时间戳格式 `1136185445000`（东八区）解析字段 `s_key` 的值作为日志时间，并以格式 `2006/01/02 15:04:05`（东九区）将日志时间转换到新字段 `d_key` 中。

> 目前支持以 seconds(秒)、milliseconds(毫秒)、microseconds(微秒) 为单位的 Unix 时间戳。

配置详情及处理结果如下：

- 输入

```
"s_key":"1645595256807"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_gotime",
      "detail": {
        "SourceKey": "s_key",
        "SourceFormat":"milliseconds",
        "SourceLocation":8,
        "DestKey":"d_key",
        "DestFormat":"2006/01/02 15:04:05.000",
        "DestLocation":9,
        "SetTime": true,
        "KeepSource": true,
        "NoKeyError": true,
        "AlarmIfFail": true
      }
    }
  ]
}
```

- 配置后结果

```
"s_key":"1645595256807"
"d_key":"2022/02/23 14:47:36.807"
```