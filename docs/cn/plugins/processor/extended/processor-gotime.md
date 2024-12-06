# Gotime

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| - | - | - | - |
| SourceKey | String | 是 | 原始字段名。 |
| SourceFormat | String | 是 | 原始时间的格式。 |
| SourceLocation | Int | 否 | 原始时间的时区。参数值为空时，表示iLogtail所在主机或容器的时区。 |
| DestKey | String | 是 | 解析后的目标字段。 |
| DestFormat | String | 是 | 解析后的时间格式。 |
| DestLocation | Int | 否 | 解析后的时区。参数值为空时，表示本机时区。 |
| SetTime | Boolean | 否 | 是否将解析后的时间设置为日志时间。true（默认值）：是。false：否。 |
| KeepSource | Boolean | 否 | 被解析后的日志中是否保留原始字段。true（默认值）：保留。false：不保留。 |
| NoKeyError | Boolean | 否 | 原始日志中无您所指定的原始字段时，系统是否报错。true（默认值）：报错。false：不报错。 |
| AlarmIfFail | Boolean | 否 | 提取日志时间失败，系统是否报错。true（默认值）：报错。false：不报错。 |

**注意** SourceFormat 和 DestFormat 的格式需要符合 [Go 时间中的 layout](https://pkg.go.dev/time#Layout)。Go 中采用基于规则的方法对时间格式进行解析，例如，时间格式中 "1" 的时间单位为月份，"15" 的时间单位为小时。因此，为了避免解析失败，推荐选择 Go 中的时间原点 "2006-01-02 15:04:05" 作为 SourceFormat 和 DestFormat 的样例时间。
或者采用以下的标准时间格式：

```go
const (
 ANSIC       = "Mon Jan _2 15:04:05 2006"
 UnixDate    = "Mon Jan _2 15:04:05 MST 2006"
 RubyDate    = "Mon Jan 02 15:04:05 -0700 2006"
 RFC822      = "02 Jan 06 15:04 MST"
 RFC822Z     = "02 Jan 06 15:04 -0700" // RFC822 with numeric zone
 RFC850      = "Monday, 02-Jan-06 15:04:05 MST"
 RFC1123     = "Mon, 02 Jan 2006 15:04:05 MST"
 RFC1123Z    = "Mon, 02 Jan 2006 15:04:05 -0700" // RFC1123 with numeric zone
 RFC3339     = "2006-01-02T15:04:05Z07:00"
 RFC3339Nano = "2006-01-02T15:04:05.999999999Z07:00"
 Kitchen     = "3:04PM"
 // Handy time stamps.
 Stamp      = "Jan _2 15:04:05"
 StampMilli = "Jan _2 15:04:05.000"
 StampMicro = "Jan _2 15:04:05.000000"
 StampNano  = "Jan _2 15:04:05.000000000"
 DateTime   = "2006-01-02 15:04:05"
 DateOnly   = "2006-01-02"
 TimeOnly   = "15:04:05"
)
```

## 样例

采集`/home/test-log/`目录下的`simple.log`文件，根据指定的配置选项提取日志信息。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
processors:
  - Type: processor_gotime
    SourceKey: "content"
    SourceFormat: "2006-01-02 15:04:05"
    SourceLocation: 8
    DestKey: "d_key"
    DestFormat: "2006/01/02 15:04:05"
    DestLocation: 9
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输入

```bash
echo "2006-01-02 15:04:05" >> simple.log
```

* 输出

```json
{
    "content":"2006-01-02 15:04:05",
    "d_key":"2006/01/02 16:04:05"
}
```
