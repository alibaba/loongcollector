# IP 地理位置

## 简介

`processor_geoip`插件可以根据指定字段中的 IP 地址，从本地 GeoIP 数据库（mmdb 格式）中查询并补充国家、省份、城市、经纬度等地理位置信息。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/geoip/processor_geoip.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

| LogGroup(v1) | EventTypeLogging | EventTypeMetric | EventTypeSpan |
| ------------ | ---------------- | --------------- | ------------- |
|      ✅      |      ✅           |      透传        |      透传      |

> v2 场景下根据 Log 事件中 SourceKey 的 IP 值补充地理位置信息（输入 Log 输出 Log），Metric/Span 事件显式透传（不处理但不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上
* 插件本身不包含 GeoIP 数据库，需通过 `DBPath` 指定 mmdb 格式的数据库文件。

## 配置参数

| 参数          | 类型   | 是否必选 | 说明                                                                                              |
| ------------- | ------ | -------- | ------------------------------------------------------------------------------------------------- |
| Type          | string | 是       | 插件类型，固定为 `processor_geoip`。                                                               |
| SourceKey     | string | 是       | 存放 IP 地址的原始字段名。取值为 `__source__` 时使用采集端本机 IP。                                |
| DBPath        | string | 是       | GeoIP 数据库文件（mmdb 格式）的绝对路径。                                                          |
| Language      | string | 否       | 地理位置名称的语言（对应数据库中的语言键）。默认 `zh-CN`。                                         |
| IPValueFlag   | bool   | 否       | 当 IP 以整数形式存储时是否按整数解析。默认 `false`（按点分十进制字符串解析）。                     |
| NoProvince    | bool   | 否       | 是否不补充省份信息。默认 `false`（补充）。                                                         |
| NoCity        | bool   | 否       | 是否不补充城市信息。默认 `false`（补充）。                                                         |
| NoCountry     | bool   | 否       | 是否不补充国家信息。默认 `false`（补充）。                                                         |
| NoCountryCode | bool   | 否       | 是否不补充国家代码信息。默认 `false`。                                                             |
| NoCoordinate  | bool   | 否       | 是否不补充经纬度信息。默认 `false`（补充）。                                                       |
| NoKeyError    | bool   | 否       | 找不到 SourceKey 时是否告警。默认 `false`。                                                        |
| NoMatchError  | bool   | 否       | IP 无效或无法匹配时是否告警。默认 `false`。                                                        |
| KeepSource    | bool   | 否       | 是否保留原始字段。默认 `true`。                                                                    |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，根据字段 `ip` 补充地理位置信息。

* 输入

```bash
echo '1.2.3.4' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_geoip
    SourceKey: content
    DBPath: /home/geoip/GeoLite2-City.mmdb
    Language: zh-CN
    KeepSource: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "content": "1.2.3.4",
    "content_country_": "美国",
    "content_country_code_": "US",
    "content_longitude_": "-97.82200000",
    "content_latitude_": "37.75100000",
    "__time__": "1657354602"
}
```

> 说明：补充字段名以 SourceKey 为前缀，实际返回的字段取决于所用 GeoIP 数据库的数据与配置的 `No*` 开关。
