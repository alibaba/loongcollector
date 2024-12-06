# 路由

## 简介

您可以选择根据pipeline event group的属性将group发送到不同的flusher。

## 限制

仅限使用原生处理插件且所有flusher均支持发送路由能力的场景。

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  enum  |  是  |  /  |  event_type或tag。  |

- 当Type取值为event_type时，表示根据group的事件属性进行路由，支持参数如下：

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Value  |  enum  |  是  |  /  |  log或metric或trace。  |

- 当Type取值为tag时，表示根据group的tag中的指定字段取值进行路由，支持参数如下：

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Key  |  string  |  是  |  /  |  tag的键。  |
|  Value  |  string  |  是  |  /  |  tag的值。  |

## 样例

采集k8s集群中所有容器内`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将default命名空间下的日志发送到sls的test_logstore_1，test命名空间下的日志发送到test_logstore_2。

``` yaml
enable: true
inputs:
  - Type: input_file
    EnableContainerDiscovery: true
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_sls
    Region: cn-hangzhou
    Endpoint: cn-hangzhou.log.aliyuncs.com
    Project: test_project
    Logstore: test_logstore_1
    Match:
      Type: tag
      Key: _namespace_
      Value: default
  - Type: flusher_sls
    Region: cn-hangzhou
    Endpoint: cn-hangzhou.log.aliyuncs.com
    Project: test_project
    Logstore: test_logstore_2
    Match:
      Type: tag
      Key: _namespace_
      Value: test
```
