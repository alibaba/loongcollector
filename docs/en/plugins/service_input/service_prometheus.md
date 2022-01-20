# service_prometheus
## Description
prometheus scrape plugin for logtail, use vmagent lib
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|Yaml|string|the prometheus configuration content, more details please see [here](https://prometheus.io/docs/prometheus/latest/configuration/configuration/)|""|
|ConfigFilePath|string|the prometheus configuration path, and the param would be ignored when Yaml param is configured.|""|
|AuthorizationPath|string|the prometheus authorization path, only using in authorization files. When Yaml param is configured, the default value is the current binary path. However, the default value is the ConfigFilePath directory when ConfigFilePath is working.|""|