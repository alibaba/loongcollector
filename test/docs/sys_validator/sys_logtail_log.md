# sys_logtail_log
## Description
this is a LogtailPlugin log validator to check the behavior of LogtailPlugin
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|expect_contains_log_times|map[string]int|the times of the expected logs in LogtailPlugin.|null|
|main_log|bool|the C part log would be checked when configured true, otherwise is Go part log.|false|