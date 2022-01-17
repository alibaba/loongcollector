# sys_alarm_checker
## Description
this is a alarm log validator to check received alarm log from subscriber
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|project|string|the alarm project|""|
|logstore|string|the alarm logstore|""|
|alarm_type|string|the alarm type|""|
|alarm_msg_regexp|string|the alarm msg regexp|""|
|expect_minimum_count|int|the alarm minimum count|0|