日志时间提取（Strptime）
----

该插件可从指定字段中提取日志时间，时间格式为 [Linux strptime](http://man7.org/linux/man-pages/man3/strptime.3.html)。

#### 参数说明
插件类型（type）为 `processor_strptime`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|SourceKey|string|必选|源Key，为空不生效。|
|Format|string|必选|解析指定字段所使用的时间格式。|
|AdjustUTCOffset|bool|可选|是否对时间时区进行调整，默认为 false。|
|UTCOffset|int|可选|用于调整的时区偏移秒数，如 28800 表示东八区。|
|AlarmIfFail|bool|可选|提取失败时是否告警，默认为 true。|
|KeepSource|bool|可选|是否保留源字段，默认为true。|

#### 示例 1
以格式 `%Y/%m/%d %H:%M:%S` 解析字段 `log_time` 的值作为日志时间，时区使用机器时区，此处假设为东八区。

配置详情及处理结果如下：

- 输入

```
"log_time":"2016/01/02 12:59:59"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_strptime",
      "detail": {
        "SourceKey": "log_time",
        "Format": "%Y/%m/%d %H:%M:%S"
      }
    }
  ]
}
```

- 配置后结果

```
"log_time":"2016/01/02 12:59:59"
Log.Time = 1451710799
```

#### 示例 2
时间格式同示例 1，但是配置中指定日志时区为东七区。

配置详情及处理结果如下：

- 输入

```
"log_time":"2016/01/02 12:59:59"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_strptime",
      "detail": {
        "SourceKey": "log_time",
        "Format": "%Y/%m/%d %H:%M:%S",
        "AdjustUTCOffset": true,
        "UTCOffset": 25200
      }
    }
  ]
}
```

- 配置后结果

```
"log_time":"2016/01/02 12:59:59"
Log.Time = 1451714399
```