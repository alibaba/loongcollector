# sys_docker_profile
## Description
this is a system validator to monitor the CPU and RAM status of Ilogtail docker container
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|expect_every_second_maximum_cpu|float64|the maximum CPU percentage in every second, the unit is percentage.|0|
|expect_every_second_maximum_mem|string|the maximum memory cost in every second, such as 300KB.|""|