# SLS

## 简介

`flusher_sls` `flusher`插件将采集到的事件发送至SLS。

使用本插件时，必须在启动参数中配置[AK和SK](../../configuration/system-config.md)。

## 版本

[Stable](../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为flusher\_sls。  |
|  Project  |  string  |  是  |  /  |  Project名称。  |
|  Logstore  |  stirng  |  是  |  /  |  LogStrore名称。  |
|  Region  |  string  |  是  |  /  |  Project所在区域。  |
|  Endpoint  |  string  |  是  |  /  |  [SLS接入点地址](https://help.aliyun.com/document\_detail/29008.html)。  |

## 安全性说明

`flusher_sls` 默认使用 `HTTPS` 协议发送数据到 `SLS`，也可以使用[data_server_port](../../configuration/system-config.md)参数更改发送协议。

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到SLS。

``` yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_sls
    Region: cn-hangzhou
    Endpoint: cn-hangzhou.log.aliyuncs.com
    Project: test_project
    Logstore: test_logstore
```
