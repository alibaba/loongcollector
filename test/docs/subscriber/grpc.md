# grpc
## Description
this a gRPC subscriber, which is the default mock backend for Ilogtail.
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|address|string|the gRPC server address, default value is :9000|""|
|network|string|the gRPC server network, default value is tcp|""|
|delay_start|string|the delay start time duration for fault injection, such as 5s|""|