# metric_input_netping
## Description
a icmp-ping/tcp-ping plugin for logtail
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|interval_seconds|int|the interval of ping/tcping, unit is second,must large than 30|0|
|icmp|[]netping.ICMPConfig|the icmping config list, example:  {"src" : "${IP_ADDR}",  "target" : "${REMOTE_HOST}", "count" : 3}|null|
|tcp|[]netping.TCPConfig|the tcping config list, example: {"src" : "${IP_ADDR}",  "target" : "${REMOTE_HOST}", "port" : ${PORT}, "count" : 3}|null|