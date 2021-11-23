字段丢弃
---

该插件可对指定的字段（支持多个）进行丢弃。

#### 参数说明

插件类型（type）为 `processor_drop`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|DropKeys|array|必选|字符串数组，指定要从日志中删除的（多个）键。|

#### 示例
从日志中删除 `aaa1` 和 `aaa2` 字段，配置详情及处理结果如下：

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
      "type":"processor_drop",
      "detail": {
        "DropKeys": ["aaa1","aaa2"]
      }
    }
  ]
}
```

- 配置后结果

```
"aaa3":"value3"
```