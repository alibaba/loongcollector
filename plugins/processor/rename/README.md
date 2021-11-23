字段重命名
---

该插件可对指定的字段（支持多个）进行重命名。

#### 参数说明

插件类型（type）为 `processor_rename`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|NoKeyError|bool|可选|无匹配的key是否记录，默认false。|
|SourceKeys|array|必选|指定要从日志中重命名的源key（多个），字符串数组。|
|DestKeys|array|必选|指定要从日志中重命名的目标key（多个），字符串数组。|

#### 示例
将字段 `aaa1` 和 `aaa2` 重命名为 `bbb1` 和 `bbb2`，配置详情及处理结果如下：

- 输入

```
"aaa1":"value1"
"aaa2":"value2"
"aaa3":"value3"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_rename",
      "detail": {
        "SourceKeys": ["aaa1","aaa2"],
        "DestKeys": ["bbb1","bbb2"],
        "NoKeyError": true
      }
    }
  ]
}
```

- 配置后结果

```
"bbb1":"value1"
"bbb2":"value2"
"aaa3":"value3"
```