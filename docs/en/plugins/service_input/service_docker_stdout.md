# service_docker_stdout
## Description
the container stdout input plugin for iLogtail, which supports docker and containerd.
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|IncludeLabel|map[string]string|include container label for selector. [Deprecated： use IncludeContainerLabel and IncludeK8sLabel instead]|null|
|ExcludeLabel|map[string]string|exclude container label for selector. [Deprecated： use ExcludeContainerLabel and ExcludeK8sLabel instead]|null|
|IncludeEnv|map[string]string|the container would be selected when it is matched by any environment rules. Furthermore, the regular expression starts with '^' is supported as the env value, such as 'ENVA:^DE.*$'' would hit all containers having any envs starts with DE.|null|
|ExcludeEnv|map[string]string|the container would be excluded when it is matched by any environment rules. Furthermore, the regular expression starts with '^' is supported as the env value, such as 'ENVA:^DE.*$'' would hit all containers having any envs starts with DE.|null|
|IncludeContainerLabel|map[string]string|the container would be selected when it is matched by any container labels. Furthermore, the regular expression starts with '^' is supported as the label value, such as 'LABEL:^DE.*$'' would hit all containers having any labels starts with DE.|null|
|ExcludeContainerLabel|map[string]string|the container would be excluded when it is matched by any container labels. Furthermore, the regular expression starts with '^' is supported as the label value, such as 'LABEL:^DE.*$'' would hit all containers having any labels starts with DE.|null|
|IncludeK8sLabel|map[string]string|the container of pod would be selected when it is matched by any include k8s label rules. Furthermore, the regular expression starts with '^' is supported as the value to match pods.|null|
|ExcludeK8sLabel|map[string]string|the container of pod would be excluded when it is matched by any exclude k8s label rules. Furthermore, the regular expression starts with '^' is supported as the value to exclude pods.|null|
|ExternalEnvTag|map[string]string|extract the env value as the log tags for one container, such as the value of ENVA would be appended to the 'taga' of log tags when configured 'ENVA:taga' pair.|null|
|ExternalK8sLabelTag|map[string]string|extract the pod label value as the log tags for one container, such as the value of LABELA would be appended to the 'taga' of log tags when configured 'LABELA:taga' pair.|null|
|FlushIntervalMs|int|the interval of container discovery，and the timeunit is millisecond. Default value is 3000.|3000|
|ReadIntervalMs|int|the interval of read stdout log，and the timeunit is millisecond. Default value is 1000.|1000|
|SaveCheckPointSec|int|the interval of save checkpoint，and the timeunit is second. Default value is 60.|60|
|BeginLineRegex|string|the regular expression of begin line for the multi line log.|""|
|BeginLineTimeoutMs|int|the maximum timeout milliseconds for begin line match. Default value is 3000.|3000|
|BeginLineCheckLength|int|the prefix length of log line to match the first line. Default value is 10240.|10240|
|MaxLogSize|int|the maximum log size. Default value is 512*1024, a.k.a 512K.|524288|
|CloseUnChangedSec|int|the reading file would be close when the interval between last read operation is over {CloseUnChangedSec} seconds. Default value is 60.|60|
|StartLogMaxOffset|int64|the first read operation would read {StartLogMaxOffset} size history logs. Default value is 128*1024, a.k.a 128K.|131072|
|Stdout|bool|collect stdout log. Default is true.|true|
|Stderr|bool|collect stderr log. Default is true.|true|
|LogtailInDocker|bool|the logtail running mode. Default is true.|true|
|K8sNamespaceRegex|string|the regular expression of kubernetes namespace to match containers.|""|
|K8sPodRegex|string|the regular expression of kubernetes pod to match containers.|""|
|K8sContainerRegex|string|the regular expression of kubernetes container to match containers.|""|