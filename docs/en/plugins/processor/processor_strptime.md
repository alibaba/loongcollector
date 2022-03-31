# processor_strptime
## Description
Processor to extract time from log: strptime(SourceKey, Format)
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|SourceKey|string|The source key prepared to be parsed by strptime.|"time"|
|Format|string|The source key formatted pattern, more details please see [here](https://golang.org/pkg/time/#Time.Format).|""|
|KeepSource|bool|Optional. Specifies whether to keep the source key in the log content after the processing.|true|
|AdjustUTCOffset|bool|Optional. Specifies whether to modify the time zone.|false|
|UTCOffset|int|Optional. The UTCOffset is used to modify the log time zone. For example, the value 28800 indicates that the time zone is modified to UTC+8.|54000|
|AlarmIfFail|bool|Optional. Specifies whether to trigger an alert if the time information fails to be extracted.|true|
|EnablePreciseTimestamp|bool|Optional. Specifies whether to enable precise timestamp.|false|
|PreciseTimestampKey|string|Optional. The generated precise timestamp key.|"precise_timestamp"|
|PreciseTimestampUnit|string|Optional. The generated precise timestamp unit.|"ms"|