# Gotime

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| - | - | - | - |
| SourceKey | String | 是 | 原始字段名。 |
| SourceFormat | String | 是 | 原始时间的格式。 |
| SourceLocation | Int | 是 | 原始时间的时区。参数值为空时，表示iLogtail所在主机或容器的时区。 |
| DestKey | String | 是 | 解析后的目标字段。 |
| DestFormat | String | 是 | 解析后的时间格式。 |
| DestLocation | Int | 否 | 解析后的时区。参数值为空时，表示本机时区。 |
| SetTime | Boolean | 否 | 是否将解析后的时间设置为日志时间。true（默认值）：是。false：否。 |
| KeepSource | Boolean | 否 | 被解析后的日志中是否保留原始字段。true（默认值）：保留。false：不保留。 |
| NoKeyError | Boolean | 否 | 原始日志中无您所指定的原始字段时，系统是否报错。true（默认值）：报错。false：不报错。 |
| AlarmIfFail | Boolean | 否 | 提取日志时间失败，系统是否报错。true（默认值）：报错。false：不报错。 |

## 样例

采集当前路径下的`simple.log`文件，根据指定的配置选项提取日志信息。

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: .
    FilePattern: simple.log
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
