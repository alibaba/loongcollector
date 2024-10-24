# 采集配置

`iLogtail`流水线是通过采集配置文件来定义的，每一个采集配置文件对应一条流水线。

## 格式

采集配置文件支持json和yaml文件格式，每个采集配置的一级字段如下：

| **参数**                           | **类型**     | **是否必填** | **默认值** | **说明**                          |
|----------------------------------|------------|----------|---------|---------------------------------|
| enable                           | bool       | 否        | true    | 是否使用当前配置。                       |
| global                           | object     | 否        | 空       | 全局配置。                           |
| global.StructureType             | string     | 否        | v1      | 流水线版本为v1或v2。               |
| global.InputIntervalMs           | int        | 否        | 1000    | MetricInput采集间隔，单位毫秒。               |
| global.InputMaxFirstCollectDelayMs| int       | 否        | 10000   | MetricInput启动后, 第一次采集随机等待时长上限，如果采集间隔更小，则以采集间隔为准               |
| global.EnableTimestampNanosecond | bool       | 否        | false   | 否启用纳秒级时间戳，提高时间精度。               |
| inputs                           | \[object\] | 是        | /       | 输入插件列表。目前只允许使用1个输入插件。           |
| processors                       | \[object\] | 否        | 空       | 处理插件列表。                         |
| aggregators                      | \[object\] | 否        | 空       | 聚合插件列表。目前最多只能包含1个聚合插件，所有输出插件共享。 |
| flushers                         | \[object\] | 是        | /       | 输出插件列表。至少需要包含1个输出插件。            |
| extenstions                      | \[object\] | 否        | 空       | 扩展插件列表。                         |

其中，inputs、processors、aggregators、flushers和extenstions中可包含任意数量的[插件](../plugins/overview.md)。

## 组织形式

本地的采集配置文件默认均存放在`./config/local`目录下，每个采集配置一个文件，文件名即为采集配置的名称。

## 热加载

采集配置文件支持热加载，当您在`./config/local`目录下新增或修改已有配置文件，iLogtail将自动感知并重新加载配置。生效等待时间最长默认为10秒，可通过启动参数`config_scan_interval`进行调整。

## 示例

一个典型的采集配置如下所示：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/reg.log
processors:
  - Type: processor_parse_regex_native
    SourceKey: content
    Regex: (\d*-\d*-\d*)\s(.*)
    Keys:
      - time
      - msg
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

其它常见的采集配置可参考[`example_config`](../../../example_config/)目录.
