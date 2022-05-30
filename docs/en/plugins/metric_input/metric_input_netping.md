# metric_input_netping
## Description
a icmp-ping/tcp-ping plugin for logtail
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|disable_dns_metric|bool|disable dns resolve metric, default is false| false|
|timeout_seconds|int|the timeout of ping/tcping, unit is second,must large than or equal 1, less than  30, default is 5|5|
|interval_seconds|int|the interval of ping/tcping, unit is second,must large than or equal 5, less than 86400 and timeout_seconds, default is 60|60|
|icmp|[]netping.ICMPConfig|the icmping config list, example:  {"src" : "${IP_ADDR}",  "target" : "${REMOTE_HOST}", "count" : 3}|null|
|tcp|[]netping.TCPConfig|the tcping config list, example: {"src" : "${IP_ADDR}",  "target" : "${REMOTE_HOST}", "port" : ${PORT}, "count" : 3}|null|