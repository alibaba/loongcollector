JSON 处理
---

该插件可对指定的字段进行 JSON 展开。

#### 参数说明

插件类型（type）为 `processor_json`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|SourceKey|string|必选|要展开的源key。|
|NoKeyError|bool|可选|无匹配key是否记录，默认为true。|
|ExpandDepth|int|可选|非负整数，展开深度，0是不限制，1是当前层，默认为0。|
|ExpandConnector|string|可选|展开时的连接符，可以为空，默认为_。|
|Prefix|string|可选|json解析出Key附加的前缀，默认为空。|
|KeepSource|bool|可选|是否保留源字段，默认为true。|
|UseSourceKeyAsPrefix|bool|可选|源key是否作为所有展开key的前缀，默认为false。|

#### 示例

对字段 `s_key` 进行 JSON 展开，配置详情及处理结果如下：

- 输入

```
"s_key":"{\"k1\":{\"k2\":{\"k3\":{\"k4\":{\"k51\":\"51\",\"k52\":\"52\"},\"k41\":\"41\"}}})"
```

- 配置详情

```json
{
  "processors":[
    {
      "type":"processor_json",
      "detail": {
        "SourceKey": "s_key",
        "NoKeyError":true,
        "ExpandDepth":0,
        "ExpandConnector":"-",
        "Prefix":"j",
        "KeepSource": false,
        "UseSourceKeyAsPrefix": true
      }
    }
  ]
}
```

- 配置后结果

```
"js_key-k1-k2-k3-k4-k51":"51"
"js_key-k1-k2-k3-k4-k52":"52"
"js_key-k1-k2-k3-k41":"41"
```