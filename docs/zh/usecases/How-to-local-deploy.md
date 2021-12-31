# iLogtail本地配置模式部署(For Kafka Flusher)

阿里已经正式[开源](https://mp.weixin.qq.com/s/hcCYBZ0qvs8q4Wp1zFt-lg)了可观测数据采集器iLogtail。作为阿里内部可观测数据采集的基础设施，iLogtail承载了阿里巴巴集团、蚂蚁的日志、监控、Trace、事件等多种可观测数据的采集工作。
​

iLogtail作为阿里云[SLS](https://help.aliyun.com/document_detail/95923.html)的采集Agent，一般情况下都是配合SLS进行使用，通常采集配置都是通过SLS控制台或API进行的。那是否可以在不依赖于SLS的情况下使用iLogtail呢？
​

本文将会详细介绍如何在不依赖于SLS控制台的情况下，进行iLogtail本地配置模式部署，并将json格式的日志文件采集到非[SLS](https://help.aliyun.com/document_detail/95923.html)（例如Kafka等）。
# 场景
采集`/root/bin/input_data/json.log`（单行日志json格式），并将采集到的日志写入本地部署的kafka中。
# 前提条件
kafka本地安装完成，并创建名为`logtail-flusher-kafka`的topic。部署详见[链接](https://kafka.apache.org/quickstart)。
# 安装ilogtail
[下载](https://github.com/alibaba/ilogtail/releases)最新的ilogtail版本，并解压。
```shell
# 解压tar包
$ tar zxvf logtail-linux64.tar.gz

# 查看目录结构
$ ll logtail-linux64
drwxr-xr-x   3 500 500  4096 bin
drwxr-xr-x 184 500 500 12288 conf
-rw-r--r--   1 500 500   597 README
drwxr-xr-x   2 500 500  4096 resources

# 进入bin目录
$ cd logtail-linux64/bin
$ ll
-rwxr-xr-x 1 500 500 10052072 ilogtail_1.0.28 # ilogtail可执行文件
-rwxr-xr-x 1 500 500     4191 ilogtaild  
-rwxr-xr-x 1 500 500     5976 libPluginAdapter.so
-rw-r--r-- 1 500 500 89560656 libPluginBase.so
-rwxr-xr-x 1 500 500  2333024 LogtailInsight
```
# 采集配置
## 配置格式
针对json格式的日志文件采集到本地kafa的配置格式：
```shell
{
    "metrics": {
	       "{config_name1}" : {
            "enable": true,
            "category": "file",
            "log_type": "json_log",
            "log_path": "/root/bin/input_data",
            "file_pattern": "json.log",
            "plugin": {
                "processors": [ {
                    "detail": {
                        "ExpandConnector": "",
                        "ExpandDepth": 1,
                        "SourceKey": "content",
                        "KeepSource": false
                    },
                    "type": "processor_json"
                }],
                "flushers":[
                {
                    "type": "flusher_kafka",
                    "detail": {
                        "Brokers":["localhost:9092"],
                        "Topic": "logtail-flusher-kafka"
                    }
                }]
            },
            "version": 1
		   },
		   "{config_name2}" : {
		       ...
		   }
	}
}
```
详细格式说明：

- 文件最外层的key为`metrics`，内部为各个具体的采集配置。
- 采集配置的key为配置名，改名称需保证在本文件中唯一。建议命名："##1.0##采集配置名称"。
- 采集配置value内部为具体采集参数配置，其中关键参数以及含义如下：

| 参数名 | 类型 | 描述 |
| --- | --- | --- |
| enable | bool | 该配置是否生效，为false时该配置不生效。 |
| category | string | 文件采集场景取值为"file"。 |
| log_type | string | log类型。json采集场景下取值json_log。 |
| log_path | string | 采集路径。 |
| file_pattern | string | 采集文件。 |
| plugin | object | 具体采集配置，为json object，具体配置参考下面说明 |
| version | int | 该配置版本号，建议每次修改配置后加1 |

- plugin 字段为json object，为具体输入源以及处理方式配置：

| 配置项 | 类型 | 描述 |
| --- | --- | --- |
| processors | object array | 处理方式配置，具体请参考[链接](https://help.aliyun.com/document_detail/196153.html)。 processor_json：将原始日志按照json格式展开。 |
| flushers | object array | flusher_stdout：采集到标准输出，一般用于调试场景; flusher_kafka：采集到kafka。 |

## 完整配置样例

- 进入bin目录，创建及`sys_conf_dir`文件夹及`ilogtail_config.json`文件。
```shell
# 1. 创建sys_conf_dir
$ mkdir sys_conf_dir

# 2. 创建ilogtail_config.json并完成配置。
##### logtail_sys_conf_dir取值为：$pwd/sys_conf_dir/
##### config_server_address固定取值，保持不变。
$ pwd
/root/bin/logtail-linux64/bin
$ cat ilogtail_config.json
{
     "logtail_sys_conf_dir": "/root/bin/logtail-linux64/bin/sys_conf_dir/",  

     "config_server_address" : "http://logtail.cn-zhangjiakou.log.aliyuncs.com"
}

# 3. 此时的目录结构
$ ll
-rwxr-xr-x 1  500  500 ilogtail_1.0.28
-rw-r--r-- 1 root root ilogtail_config.json
-rwxr-xr-x 1  500  500 ilogtaild
-rwxr-xr-x 1  500  500 libPluginAdapter.so
-rw-r--r-- 1  500  500 libPluginBase.so
-rwxr-xr-x 1  500  500 LogtailInsight
drwxr-xr-x 2 root root sys_conf_dir
```


- 在`sys_conf_dir`下创建采集配置文件`user_local_config.json`。

说明：`json_log`场景下，`user_local_config.json`仅需修改采集路径相关参数`log_path`、`file_pattern`即可，其他参数保持不变。
```shell
$ cat sys_conf_dir/user_local_config.json
{
    "metrics":
    {
        "##1.0##kafka_output_test":
        {
            "category": "file",
            "log_type": "json_log",
            "log_path": "/root/bin/input_data",
            "file_pattern": "json.log",
            "create_time": 1631018645,
            "defaultEndpoint": "",
            "delay_alarm_bytes": 0,
            "delay_skip_bytes": 0,
            "discard_none_utf8": false,
            "discard_unmatch": false,
            "docker_exclude_env":
            {},
            "docker_exclude_label":
            {},
            "docker_file": false,
            "docker_include_env":
            {},
            "docker_include_label":
            {},
            "enable": true,
            "enable_tag": false,
            "file_encoding": "utf8",
            "filter_keys":
            [],
            "filter_regs":
            [],
            "group_topic": "",
            "plugin":
            {
                "processors":
                [
                    {
                        "detail":
                        {
                            "ExpandConnector": "",
                            "ExpandDepth": 1,
                            "SourceKey": "content",
                            "KeepSource": false
                        },
                        "type": "processor_json"
                    }
                ],
                "flushers":
                [
                    {
                        "type": "flusher_kafka",
                        "detail":
                        {
                            "Brokers":
                            [
                                "localhost:9092"
                            ],
                            "Topic": "logtail-flusher-kafka"
                        }
                    }
                ]
            },
            "local_storage": true,
            "log_tz": "",
            "max_depth": 10,
            "max_send_rate": -1,
            "merge_type": "topic",
            "preserve": true,
            "preserve_depth": 1,
            "priority": 0,
            "raw_log": false,
            "aliuid": "",
            "region": "",
            "project_name": "",
            "send_rate_expire": 0,
            "sensitive_keys":
            [],
            "shard_hash_key":
            [],
            "tail_existed": false,
            "time_key": "",
            "timeformat": "",
            "topic_format": "none",
            "tz_adjust": false,
            "version": 1,
            "advanced":
            {
                "force_multiconfig": false,
                "tail_size_kb": 1024
            }            
        }
    }
}
```
# 启动ilogtail
```shell
########## 终端模式运行 ##########
$ ./ilogtail_1.0.28 --ilogtail_daemon_flag=false 

########## 也可以选择daemon模式运行 ##########
$ ./ilogtail_1.0.28
$ ps -ef|grep logtail
root       48453       1   ./ilogtail_1.0.28
root       48454   48453   ./ilogtail_1.0.28

```
# 采集场景模拟
往`/root/bin/input_data/json.log`中构造json格式的数据，代码如下：
```shell
$ echo '{"seq": "1", "action": "kkkk", "extend1": "", "extend2": "", "type": "1"}' >> json.log
$ echo '{"seq": "2", "action": "kkkk", "extend1": "", "extend2": "", "type": "1"}' >> json.log
```
消费topic为`logtail-flusher-kafka`中的数据。
```shell
$ bin/kafka-console-consumer.sh --bootstrap-server localhost:9092 --topic logtail-flusher-kafka
{"Time":1640862641,"Contents":[{"Key":"__tag__:__path__","Value":"/root/bin/input_data/json.log"},{"Key":"seq","Value":"1"},{"Key":"action","Value":"kkkk"},{"Key":"extend1","Value":""},{"Key":"extend2","Value":""},{"Key":"type","Value":"1"}]}
{"Time":1640862646,"Contents":[{"Key":"__tag__:__path__","Value":"/root/bin/input_data/json.log"},{"Key":"seq","Value":"2"},{"Key":"action","Value":"kkkk"},{"Key":"extend1","Value":""},{"Key":"extend2","Value":""},{"Key":"type","Value":"1"}]}

```
# 本地调试
为了快速方便验证配置是否正确，可以将采集到的日志打印到标准输出完成快速的功能验证。
​

替换本地采集配置`plugin-flushers`为`flusher_stdout`，并以终端模式运行$ `./ilogtail_1.0.28 --ilogtail_daemon_flag=false`，即可将采集到的日志打印到标准输出快速进行本地调试。
```shell
{
    "type": "flusher_stdout",
    "detail":
    {
        "OnlyStdout": true
    }
}
```
