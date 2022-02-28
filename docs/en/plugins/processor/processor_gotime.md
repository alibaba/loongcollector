# processor_gotime
## Description
the time format processor to parse time field with golang format pattern. More details please see [here](https://golang.org/pkg/time/#Time.Format)
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|SourceKey|string|the source key prepared to be formatted|""|
|SourceFormat|string|the source key formatted pattern, more details please see [here](https://golang.org/pkg/time/#Time.Format). Furthermore，there are 3 fixed pattern supported to parse timestamp, which are 'seconds','milliseconds' and 'microseconds'.|""|
|SourceLocation|int|the source key time zone, such beijing timezone is 8. And the parameter would be ignored when 'SourceFormat' configured with timestamp format pattern.|0|
|DestKey|string|the generated key name.|""|
|DestFormat|string|the generated key formatted pattern, more details please see [here](https://golang.org/pkg/time/#Time.Format).|""|
|DestLocation|int|the generated key time zone, such beijing timezone is 8.|0|
|SetTime|bool|Whether to config the unix time of the source key to the log time. |true|
|KeepSource|bool|Whether to keep the source key in the log content after the processing.|true|
|NoKeyError|bool|Whether to alarm when not found the source key to parse and format.|true|
|AlarmIfFail|bool|Whether to alarm when the source key is failed to parse.|true|