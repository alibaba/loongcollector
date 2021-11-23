字段添加
---

该插件可添加指定的字段（支持多个）。

#### 参数说明

插件类型（type）为 `processor_add_fields`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|Fields|map|必选|键值对，指定要添加的（多个）键值。|
|IgnoreIfExist|bool|可选|存在相同的key是否忽略，默认为false。|

#### 示例
添加两个字段 `aaa2` 和 `aaa3`，配置详情及处理结果如下：

- 输入

```
"aaa1":"value1"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_add_fields",
      "detail": {
        "Fields": {
          "aaa2": "value2",
          "aaa3": "value3"
        }
      }
    }
  ]
}
```

- 配置后结果

```
"aaa1":"value1"
"aaa2":"value2"
"aaa3":"value3"
```