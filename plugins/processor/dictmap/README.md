字段值映射处理
---

该插件可对指定字段的值查表映射

#### 参数说明

插件类型（type）为 `processor_dict_map`。


|参数|类型|必选或可选|参数说明|
|----|----|----|----|
|DictFilePath|string|可选|本地的csv字典文件，该csv文件分隔符为`,`,字段引用标志为`"`。映射规则为表格第一列为原数据，表格第二列为目标数据。默认为空。|
|MapDict|Map|可选|映射字典，规则为将`键`映射到`值`。若映射字典较小，可直接在设置中的填写，不必提供本地文件，默认为空。当`DictFilePath`非空，忽略设置中的该字段，只使用`DictFilePath`数据。|
|SourceKey|string|必选|原数据在日志中的字段名。若有多个同名输入，默认只处理第一个。|
|DestKey|string|可选|映射后数据在日志中的字段名，默认为和SourceKey一致。|
|HandleMissing|bool|可选|是否处理日志中缺失目标字段的情况，默认为`false`不处理。|
|Missing|string|可选|处理日志中缺失目标字段的情况时的填充值，默认为`"Unknown"`。|
|Mode|string|可选|当映射后字段于原日志中存在时的处理方法，默认为`"overwrite"`即覆写原字段，若设置为`"fill"`则不再覆写目标字段。|
|MaxDictSize|int|可选|映射字典的最大大小，默认`1000`，即最大存储1000条映射规则。若希望限制插件对内存的占用，可以将本设置调小。| 

#### 配置文件示例

- 插件设置示例

在`config.json`中:

```json
"processors":[
    {
      "type":"processor_dict_map",
         "detail": {
            "DictFilePath" : "/home/ipExample.csv",
            "MapDict":{
              "1":"TCP",
              "2":"UDP",
              "3":"HTTP",
              "*":"Unknown"
              },
            "SourceKey":"_ip_",
            "DestKey": "_processed_ip_",
            "Mode":"overwrite", 
            "HandleMissing": true,
            "Missing": "Not Detected"
      }
    }
  ],
```


注意到这里同时设置了两种映射规则来源：本地文件`ipExample.csv`和配置文件`{"1":"TCP","2":"UDP","3":"HTTP","*":"Unknown"}`。当两者同时设置时，默认使用配置文件，即`ipExample.csv`，同时忽略配置文件的内容。

此配置将检查日志中`_ip_`字段的值是否存在。若不存在，由于设置：`"HandleMissing": true,`，故会处理这种缺失，将在日志中增加`"_processed_ip_"`字段并填充缺失值为`"Not Detected"`。若存在，同样将在日志中增加`"_processed_ip_"`字段，将查映射字典`ipExample.csv`得到的值填入该字段内。若查表无,则不处理，日志不发生改变。


- 映射表文件示例

映射表必须为UFT-8格式输入，每行两列，，将第一列的内容映射到第二列。

如`ipExample.csv`内容为：

```csv
"127.0.0.1","LocalHost-LocalHost"
"192.168.0.1","default login"
```

在配置Logtail时，请将映射表文件（如上文的`ipExample.csv`）放在用户目录下（如`/home/ipExample.csv`）而非Logtail的配置文件夹`ilogtail`中，因`ilogtail`文件夹会在Logtail升级时被删除。具体配置规则请参考[使用Logtail插件处理数据](https://help.aliyun.com/document_detail/64957.html) 。


#### 处理日志示例

当使用如上设置，连续对日志进行映射，处理结果如下：

- 连续日志输入

```
{"_ip_":"192.168.0.1","Index":"900000003","__time__":"1627004587"}
{"_ip_":"255.255.255.255","Index":"3","__time__":"1627004587"}
{"_ip_":"192.168.0.1","Index":"900000004","__time__":"1627004588"}
{"_ip_":"255.255.255.255","Index":"4","__time__":"1627004588"}
{"_ip_":"127.0.0.1","Index":"100000004","__time__":"1627004588"}
{"_ip_":"127.0.0.1","Index":"100000005","__time__":"1627004589"}
{"Index":"100000006","__time__":"1627004589"}
```


- 配置后结果

```
{"_ip_":"192.168.0.1",,"Index":"900000003",,"_processed_ip_":"default login","__time__":"1627004587"}
{"_ip_":"255.255.255.255","Index":"3","__time__":"1627004587"}
{"_ip_":"192.168.0.1","Index":"900000004",,"_processed_ip_":"default login","__time__":"1627004588"}
{"_ip_":"255.255.255.255","Index":"4","__time__":"1627004588"}
{"_ip_":"127.0.0.1","Index":"100000004","_processed_ip_":"LocalHost-LocalHost","__time__":"1627004588"}
{"_ip_":"127.0.0.1","Index":"100000005","_processed_ip_":"LocalHost-LocalHost","__time__":"1627004589"}
{"Index":"100000006","_processed_ip_":"Not Detected","__time__":"1627004589"}
```