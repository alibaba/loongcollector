字段打包
---

该插件可添加指定的字段（支持多个）以 JSON 格式打包成单个字段。

#### 参数说明

插件类型（type）为 `processor_packjson`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|SourceKeys|array|必选|字符串数组，需要打包的key。|
|DestKey|string|必选|目标key。|
|KeepSource|bool|可选|是否保留源字段，默认为 true。|
|AlarmIfIncomplete|bool|可选|是否在不存在任何源字段时告警，默认为 true。|

#### 示例
将指定的 `a`、`b` 两个字段打包成 JSON 字段 `d_key`，配置详情及处理结果如下：

- 输入

```
"a":"1"
"b":"2"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_packjson",
      "detail": {
        "SourceKeys": ["a","b"],
        "DestKey":"d_key",
        "KeepSource":true,
        "AlarmIfEmpty":true
      }
    }
  ]
}
```

- 配置后结果

```
"a":"1"
"b":"2"
"d_key":"{\"a\":\"1\",\"b\":\"2\"}"
```