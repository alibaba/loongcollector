# 日志时间提取（Strptime）

## 简介

`processor_strptime`插件可从指定字段中提取日志时间，时间格式为 [Linux strptime](http://man7.org/linux/man-pages/man3/strptime.3.html)。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.7.0 及以上

## 配置参数

| 参数                   | 类型   | 是否必选 | 参数说明                                                                                                                                                 |
| ---------------------- | ------ | -------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Type                   | String | 是       | 插件类型，固定为`processor_strptime`。                                                                                                                   |
| SourceKey              | String | 是       | 源 Key，为空不生效。                                                                                                                                     |
| Format                 | String | 是       | 解析指定字段所使用的时间格式。                                                                                                                           |
| AdjustUTCOffset        | bool   | 否       | 是否对时间时区进行调整，默认为 false。                                                                                                                   |
| UTCOffset              | int    | 否       | 用于调整的时区偏移秒数，如 28800 表示东八区。                                                                                                            |
| AlarmIfFail            | bool   | 否       | 提取失败时是否告警，默认为 true。                                                                                                                        |
| KeepSource             | bool   | 否       | 是否保留源字段，默认为 true。                                                                                                                            |
| EnablePreciseTimestamp | bool   | 否       | 是否提取高精度时间。设置为 true 后，该插件会将 SourceKey 参数对应的字段值解析为毫秒级别的时间戳，并存入 PreciseTimestampKey 参照中对应的字段。默认为否。 |
| PreciseTimestampKey    | String | 否       | 保存高精度时间戳的字段。默认值为 precise_timestamp 字段。                                                                                                |
| PreciseTimestampUnit   | String | 否       | 高精度时间戳的单位。默认值为 ms。取值包括 ms（毫秒）、us（微秒）、ns（纳秒）。                                                                           |

## 示例

### 示例 1

以格式 `%Y/%m/%d %H:%M:%S` 解析字段 `log_time` 的值作为日志时间，时区使用机器时区，此处假设为东八区。

配置详情及处理结果如下：

* 输入

```text
"log_time":"2016/01/02 12:59:59"
```

* 配置详情

```yaml
processors:
  - Type: processor_strptime
    SourceKey: log_time
    Format: "%Y/%m/%d %H:%M:%S"
```

* 配置后结果

```text
"log_time":"2016/01/02 12:59:59"
Log.Time = 1451710799
```

### 示例 2

时间格式同示例 1，但是配置中指定日志时区为东七区。

配置详情及处理结果如下：

* 输入

```text
"log_time":"2016/01/02 12:59:59"
```

* 配置详情

```yaml
processors:
  - Type: processor_strptime
    SourceKey: log_time
    Format: "%Y/%m/%d %H:%M:%S"
    AdjustUTCOffset: true
    UTCOffset: 25200
```

* 配置后结果

```text
"log_time":"2016/01/02 12:59:59"
Log.Time = 1451714399
```
