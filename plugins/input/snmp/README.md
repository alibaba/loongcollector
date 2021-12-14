收集SNMP协议机器信息
---

该插件支持SNMP V1，V2，V3。可以转化用户输入并对指定ip的机器执行SNMP-GET指令获取机器信息。

运行本插件需要本地已运行snmp程序，开放`snmpget`需要的端口，且可正常执行`snmptranslate`命令和`snmptable`命令。

#### 参数说明

插件类型（type）为 `service_snmp`。

|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|Targets|string list|必选|目标机器组的ip地址组|
|Port|string|可选|SNMP协议使用的端口，默认为`"161"`|
|Community|string|可选|SNMPV1,V2使用的验证方式：`community`。默认为`"public"`|
|UserName|string|可选|SNMPV3使用的验证方式：用户名。默认为空|
|AuthenticationProtocol|string|可选|SNMPV3使用的验证方式：验证协议。默认为`NoAuth`|
|AuthenticationPassphrase|string|可选|SNMPV3使用的验证方式：验证密码。当验证协议（AuthenticationProtocol）设置为 MD5 或 SHA 时，本项是必需的。默认为空|
|PrivacyProtocol|string|可选|SNMPV3使用的验证方式：隐私协议。默认为`NoPriv`|
|PrivacyPassphrase|string|可选|SNMPV3使用的验证方式：隐私协议密码。当隐私协议设置为 DES 或 AES 时，本项是必需的。默认为与用户密码一致|
|Timeout|int|可选|一次查询操作的超时时间，单位为秒。默认为`5`秒|
|Version|int|可选|SNM协议版本，可以填写`1`，`2`，`3`，默认为`2`|
|Transport|string|可选|SNMP的通讯方法，可以填写`"udp"`，`"tcp"`，默认为`"udp"`|
|MaxRepetitions|int|可选|查询超时后的重试次数，默认为`0`|
|Oids|string list|可选|对目标机器查询的Oid列表，默认为空|
|Fields|string list|可选|对目标机器查询的Fields列表。本插件会先对Fields进行翻译，查找本地MIB将之翻译为Oids并一起查询，默认为空|
|Tables|string list|可选|对目标机器查询的Tables列表。本插件会首先查询Table内所有的Field，查找本地MIB将之翻译为Oids并一起查询，默认为空| 

#### 配置文件及结果示例

##### 插件设置示例一

在`config.json`中:

```json
  "inputs":[
        {
            "type":"service_snmp",
            "detail":{
            "Targets": ["127.0.0.1"],
            "Port": "161",
            "Community": "public",
            "Timeout": 5,
            "Version":2,
            "Transport":"udp",
            "MaxRepetitions":0,
            "Oids":[
            "1.3.6.1.2.1.1.3.0",
            "1.3.6.1.2.1.1.4.0",
            "1.3.6.1.2.1.1.7.0",
            "1.3.6.1.2.1.1.1.0"
            ]
        }
    }
],
```

本示例采用了SNMPV2协议，团体名为`"public"`

采集结果：

```text
{"_target_":"127.0.0.1","_field_":"DISMAN-EXPRESSION-MIB::sysUpTimeInstance","_oid_":".1.3.6.1.2.1.1.3.0","_conversion_":"","_type_":"TimeTicks","_content_":"10522102","_targetindex_":"0"}
{"_target_":"127.0.0.1","_field_":"SNMPv2-MIB::sysContact.0","_oid_":".1.3.6.1.2.1.1.4.0","_conversion_":"","_type_":"OctetString","_content_":"Me <me@example.org>","_targetindex_":"0"}
{"_conversion_":"","_type_":"Integer","_content_":"72","_targetindex_":"0","_target_":"127.0.0.1","_field_":"SNMPv2-MIB::sysServices.0","_oid_":".1.3.6.1.2.1.1.7.0"}
{"_targetindex_":"0","_target_":"127.0.0.1","_field_":"SNMPv2-MIB::sysDescr.0","_oid_":".1.3.6.1.2.1.1.1.0","_conversion_":"","_type_":"OctetString","_content_":"Linux 4be69bf59c36 5.10.25-linuxkit #1 SMP Tue Mar 23 09:27:39 UTC 2021 x86_64"}
```

采集字段含义：

|字段|说明|
|----|----|
|`_target_`|目标收集的机器ip|
|`_targetindex_`|目标收集的机器在本插件所有目标机器组中的排序。本例中仅有一个目标机器`127.0.0.1`因此本项恒为0|
|`_field_`|目标收集的Oid经过本地MIB翻译后的内容|
|`_oid_`|目标收集的Oid|
|`_conversion_`|收集结果进行了哪种类型转换，目前支持`"ipaddr"`和`"hwaddr"`两种，分别会对ip地址和设备地址进行转换。若为空则未发生类型转换|
|`_type_`|收集到的结果的类型|
|`_content_`|收集到的结果的内容|

##### 插件设置示例二

在`config.json`中:

```json
  "inputs":[
    {
        "type":"service_snmp",
        "detail":{
        "Targets":["127.0.0.1"],
        "Port": "161",
        "Community": "public",
        "Timeout": 5,
        "Version":3,
        "UserName":"snmpreadonly",
        "AuthenticationProtocol":"SHA",
        "AuthenticationPassphrase":"SecUREDpass",
        "PrivacyProtocol":"AES",
        "PrivacyPassphrase":"StRongPASS",
        "Oids":[
        "1.3.6.1.2.1.1.3.0",
        "1.3.6.1.2.1.1.1.0"
        ],
        "Fields":[
        "SNMPv2-MIB::sysContact.0",
        "SNMPv2-MIB::sysServices.0"
        ],
        "Transport":"udp"
        }
    }
],
```

本示例采用了SNMPV3协议，查询的目标机器组仅有一台机器`"127.0.0.1"`即本机。用户名为`"snmpreadonly"`, 验证协议为`"SHA"`, 用户密码为`"SecUREDpass"`, 隐私协议为`"AES"`, 隐私密码为`"StRongPASS"`。

若希望在本地复现该配置，可以运行：

```shell
net-snmp-create-v3-user -ro -A SecUREDpass -a SHA -X StRongPASS -x AES snmpreadonly
```

采集结果：

```text
{"_target_":"127.0.0.1","_field_":"DISMAN-EXPRESSION-MIB::sysUpTimeInstance","_oid_":".1.3.6.1.2.1.1.3.0","_conversion_":"","_type_":"TimeTicks","_content_":"10423593","_targetindex_":"0"}
{"_type_":"OctetString","_content_":"Linux 4be69bf59c36 5.10.25-linuxkit #1 SMP Tue Mar 23 09:27:39 UTC 2021 x86_64","_targetindex_":"0","_target_":"127.0.0.1","_field_":"SNMPv2-MIB::sysDescr.0","_oid_":".1.3.6.1.2.1.1.1.0","_conversion_":""}
{"_content_":"Me <me@example.org>","_targetindex_":"0","_target_":"127.0.0.1","_field_":"SNMPv2-MIB::sysContact.0","_oid_":".1.3.6.1.2.1.1.4.0","_conversion_":"","_type_":"OctetString"}
{"_conversion_":"","_type_":"Integer","_content_":"72","_targetindex_":"0","_target_":"127.0.0.1","_field_":"SNMPv2-MIB::sysServices.0","_oid_":".1.3.6.1.2.1.1.7.0"}
```
