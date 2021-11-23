字段追加
---

该插件可为指定字段（可以不存在）追加特定的值，且支持模板参数。

#### 参数说明

插件类型（type）为 `processor_appender`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|Key|string|必选|Key名称。|
|Value|string|必选|Value值，该值支持模板参数替换，具体模板取值参考下表。 |
|SortLabels|bool|可选|bool值，在时序场景下，如果添加了Labels，如果不符合字母序要求，需要重新排序。 |


|模板|说明|替换示例|
|----|----|----|
|`{{__ip__}}`| 替换为Logtail的IP地址 | 10.112.31.40 |
|`{{__host__}}`| 替换为Logtail的主机名 | logtail-ds-xdfaf |
|`{{$xxxx}}`| 以`$`开头则会替换为环境变量的取值 | 例如存在环境变量 `key=value`，则`{{$key}}` 为  value |

#### 示例
为 `__labels__` 追加一些本机特有的值：

- 输入

```
"__labels__":"a#$#b"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_appender",
      "detail": {
        "Key": "__labels__",
        "Value": "|host#$#{{__host__}}|ip#$#{{__ip__}}"
      }
    }
  ]
}
```

- 配置后结果

```
"__labels__":"a#$#b|host#$#david|ip#$#30.40.60.150"
```