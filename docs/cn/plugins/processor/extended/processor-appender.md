# 字段追加

## 简介

`processor_appender`插件可为指定字段（可以不存在）追加特定的值，且支持模板参数。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

插件类型（type）为 `processor_appender`。

| 参数       | 类型   | 是否必选 | 参数说明                                                                       |
| ---------- | ------ | -------- | ------------------------------------------------------------------------------ |
| Type       | String | 是       | 插件类型，固定为`processor_appender`。                                         |
| Key        | string | 是       | Key 名称。                                                                     |
| Value      | string | 是       | Value 值，该值支持模板参数替换，具体模板取值参考下表。                         |
| SortLabels | bool   | 否       | bool 值，在时序场景下，如果添加了 Labels，如果不符合字母序要求，需要重新排序。 |

| 模板           | 说明                              | 替换示例                                            |
| -------------- | --------------------------------- | --------------------------------------------------- |
| `{{__ip__}}`   | 替换为 LoongCollector 的 IP 地址         | 10.112.31.40                                        |
| `{{__host__}}` | 替换为 LoongCollector 的主机名           | logtail-ds-xdfaf                                    |
| `{{$xxxx}}`    | 以`$`开头则会替换为环境变量的取值 | 例如存在环境变量 `key=value`，则`{{$key}}` 为 value |

## 示例

为 `__labels__` 追加一些本机特有的值：

* 输入

```text
"__labels__":"a#$#b"
```

* 配置详情

```yaml
processors:
  - Type: processor_appender
    Key: "__labels__"
    Value: "|host#$#{{__host__}}|ip#$#{{__ip__}}"
```

* 配置后结果

```text
"__labels__":"a#$#b|host#$#david|ip#$#30.40.60.150"
```
