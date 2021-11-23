键值对切分
---

该插件可对指定的字段按照键值对的形式进行切分。

#### 参数说明
插件类型（type）为 `processor_split_key_value`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|SourceKey|string|必选|指定进行切分的字段名。|
|Delimiter|string|必选|键值对之间的分隔符，默认为制表符 `\t`。|
|Separator|string|必选|单个键值对内键与值之间的分隔符，默认为冒号 `:`。|
|KeepSource|bool|可选|提取完毕后是否保留源字段，默认为 true。|
|ErrIfSourceKeyNotFound|bool|可选|当指定的 SourceKey 不存在时，是否告警，默认为 true。
|DiscardWhenSeparatorNotFound|bool|可选|当 Separator 不存在时，是否丢弃该键值对，默认为 false。|
|ErrIfSeparatorNotFound|bool|可选|当 Separator 不存在时，是否告警，默认为 true。|

#### 示例
对字段 `content` 按照键值对方式提取，键值对间分隔符为制表符，键值对内分隔符为冒号。

配置详情及处理结果如下：

- 输入

```
"content": "class:main\tuserid:123456\tmethod:get\tmessage:\"wrong user\""
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_split_key_value",
      "detail": {
        "SourceKey": "content",
        "Delimiter": "\t",
        "Separator": ":",
        "KeepSource": true
      }
    }
  ]
}
```

- 配置后结果

```
"content": "class:main\tuserid:123456\tmethod:get\tmessage:\"wrong user\""
"class": "main"
"userid": "123456"
"method": "get"
"message": "\"wrong user\""
```