kafka输入源
---

该插件可从kafka采集数据到日志服务。

#### 参数说明

插件类型（type）为 `service_kafka`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|ConsumerGroup|string|必选|消费组名。|
|ClientID|string|必选|客户机ID。|
|Topics|string|必选|kafka主题。|
|Brokers|string|必选|kafka代理。|
|MaxMessageLen|int|可选|正整数，最大消息长度（1～524288）。|
|Version|string|可选|kafka版本，默认为空。|
|Offset|string|可选|初始偏移量（oldest，newest），默认oldest。|
|SASLUsername|string|可选|SASL用户名。|
|SASLPassword|string|可选|SASL密码。|

#### 示例

从kafka采集数据到日志服务，配置详情如下：

- 配置详情

```json
{
  "inputs":[
    {
      "type":"service_kafka",
      "detail":{
        "Brokers": ["localhost:9092"],
        "Topics": ["test1"],
        "ConsumerGroup": "group1",
        "ClientID": "id1",
        "Offset": "oldest"
      }
    }
  ]
}
```