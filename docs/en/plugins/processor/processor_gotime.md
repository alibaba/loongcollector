# processor_gotime

## Description

the time format processor to parse time field with golang format pattern. More details please see [here](https://golang.org/pkg/time/#Time.Format)

## Config

| Parameter | Type | Required | Description |
| - | - | - | - |
| SourceKey | String | Yes | The name of the original field. |
| SourceFormat | String | Yes | The format of the original time. |
| SourceLocation | Int | No | The time zone of the original time. If the parameter value is empty, it indicates the time zone of the host or container where iLogtail is located. |
| DestKey | String | Yes | The name of the target field after parsing. |
| DestFormat | String | Yes | The format of the parsed time. |
| DestLocation | Int | No | The time zone of the parsed time. If the parameter value is empty, it indicates the local time zone. |
| SetTime | Boolean | No | Whether to set the parsed time as the log time. true (default): yes. false: no. |
| KeepSource | Boolean | No | Whether to keep the original field in the parsed log. true (default): keep. false: not keep. |
| NoKeyError | Boolean | No | Whether to report an error if the specified original field is missing in the original log. true (default): report an error. false: not report an error. |
| AlarmIfFail | Boolean | No | Whether to report an error if parsing the log time fails. true (default): report an error. false: not report an error. |

## Example

Collect the log information from the `simple.log` file in the current path according to the specified configuration options.

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

* Input

```bash
echo "2006-01-02 15:04:05" >> simple.log
```

* Output

```json
{
    "content":"2006-01-02 15:04:05",
    "d_key":"2006/01/02 16:04:05"
}
```
