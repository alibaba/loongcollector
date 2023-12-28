# 如何采集Telegraf数据

## 前言

阿里已经正式开源了可观测数据采集器iLogtail。作为阿里内部可观测数据采集的基础设施，iLogtail承载了阿里巴巴集团、蚂蚁的日志、监控、Trace、事件等多种可观测数据的采集工作。本文将介绍iLogtail如何与Telegraf协同工作，采集指标数据。

## 采集配置

iLogtail目前采集配置全面兼容Telegraf[配置文件](https://github.com/influxdata/telegraf/blob/master/docs/CONFIGURATION.md), iLogtail 在工作时将启动2个插件进行协同工作，分别为service_telegraf 插件与service_http_server 插件，service_telegraf 负责建立iLogtail 与Telegraf 进程之间的关联，将Telegraf Agent的配置管理工作交由iLogtail 控制，而service_http_server则负责iLogtail 插件与Telegraf Agent 之间的具体的数据传输工作。

1. service_telegraf 控制插件的具体参数配置：

    | 参数 | 描述 | 默认值 |
    | --- | --- | --- |
    | Detail | yaml格式的Telegraf采集配置 |  |

2. service_http_server 采集插件的具体参数配置：

   | 参数 | 描述 | 默认值 |
   | --- | --- | --- |
   | Format | 数据格式，对于Telegraf Agent，设置参数为influx。 |  |
   | Address | 监听地址 |  |
   | ReadTimeoutSec | 读取超时时间 | 10 |
   | ShutdownTimeoutSec | 关闭超时时间 | 5 |
   | MaxBodySize | 最大传输body 体大小 | 64 *1024* 1024 |
   | UnlinkUnixSock | 启动前如果监听地址为unix socket，是否进行强制释放 | true |

以下是一个简单的采集配置：

```
[
    {
        "detail":{
            "Detail":"\n# Read Redis's basic status information\n[[inputs.redis]]\n  interval = \"30s\"\n  ## specify servers via a url matching:\n  ##  [protocol://][:password]@address[:port]\n  ##  e.g.\n  ##    tcp://localhost:6379\n  ##    tcp://:password@192.168.99.100\n  ##\n  ## If no servers are specified, then localhost is used as the host.\n  ## If no port is specified, 6379 is used\n  ## servers = [\"tcp://host:port\"]\n  servers = [\"tcp://127.0.0.1:6379\"]\n  ## specify server password\n  # password = \"s#cr@t%\"\n  # password = \"\"\n\n  ## Optional TLS Config\n  # tls_ca = \"/etc/telegraf/ca.pem\"\n  # tls_cert = \"/etc/telegraf/cert.pem\"\n  # tls_key = \"/etc/telegraf/key.pem\"\n  ## Use TLS but skip chain & host verification\n  # insecure_skip_verify = true\n  [inputs.redis.tags]\n    __metricstore__ = \"ljp-test-bj_telegraf_test\"\n    cluster = \"test\"\n  \n    key = \"value\"\n  \n\n\n[[outputs.influxdb]]\n  ## The full HTTP or UDP URL for your InfluxDB instance.\n  ##\n  ## Multiple URLs can be specified for a single cluster, only ONE of the\n  ## urls will be written to each interval.\n  # urls = [\"unix:///var/run/influxdb.sock\"]\n  # urls = [\"udp://127.0.0.1:8089\"]\n  urls = [\"unix:///var/run/ljp-test-bj_telegraf_test.sock\"]\n  tagexclude = [\"__metricstore__\"]\n  skip_database_creation = true\n  [outputs.influxdb.tagpass]\n    __metricstore__ = [\"ljp-test-bj_telegraf_test\"]\n"
        },
        "type":"service_telegraf"
    },
    {
        "detail":{
            "Address":"unix:///var/run/ljp-test-bj_telegraf_test.sock",
            "Format":"influx"
        },
        "type":"service_http_server"
    }
]
```

## 采集数据格式

iLogtail Telegraf 采集的Metrics 数据与日志同样遵循[iLogtail 的传输层协议](https://github.com/alibaba/ilogtail/blob/main/pkg/protocol/proto/sls_logs.proto) ，目前传输数据字段为以下格式，与Prometheus 采集格式相同。

| 传输字段 | 含义 |
| --- | --- |
| __name__ | 指标名，与Prometheus exporter 指标名相同。 |
| __labels__ | 指标Label 熟悉，数据格式为`{label_a_nane}#$#{label_a_value}&#124;{label_b_nane}#$#{label_b_value}`，例如`instance#$#exporter:18080&#124;job#$#prometheus` |
| __time_nano__ | 采集时间 |
| __value__ | 指标值 |

### 转换规则

Telegraf Agent 采集的数据规则格式为{type},{tags} {key&value pairs} {time}

```
mysql,host=Vm-Req-170328120400894271-tianchi113855.tc,server=rm-bp1eomqfte2vj91tkjo.mysql.rds.aliyuncs.com:3306 bytes_sent=19815071437i,com_assign_to_keycache=0i,com_alter_event=0i,com_alter_function=0i,com_alter_server=0i,com_alter_table=0i,aborted_clients=7738i,binlog_cache_use=136756i,binlog_stmt_cache_use=136759i,com_alter_procedure=0i,binlog_stmt_cache_disk_use=0i,bytes_received=6811387420i,com_alter_db_upgrade=0i,com_alter_instance=0i,aborted_connects=7139i,binlog_cache_disk_use=0i,com_admin_commands=3478164i,com_alter_db=0i,com_alter_tablespace=0i,com_alter_user=0i 1595818360000000000
```

iLogtail 会将接受到的Telegraf数据进行类Prometheus格式转换，转换规则为：

- __name__：{type}:{key}
- __labels__： {tags}
- __time_nano__：{time}
- __value__：{value}

## 本地采集Telegraf 采集Redis 指标数据实战

1. 准备Linux 环境。
1. [下载](https://github.com/alibaba/ilogtail/releases) 最新的ilogtail版本进行安装。

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

3. 创建采集配置目录。

```
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

5. 设置采集配置文件，将下列内如写入sys_conf_dir/user_local_config.json文件，下面核心配置为plugin部分，核心部分为plugin部分，此部分描述的内容主要是下发Telegraf 采集配置，将收集到的Telegraf 数据写于本地telegraf.log

```
{
    "metrics":{
        "test-telegraf":{
            "aliuid":"",
            "category":"",
            "create_time":1640692891,
            "defaultEndpoint":"",
            "delay_alarm_bytes":0,
            "enable":true,
            "enable_tag":false,
            "filter_keys":[

            ],
            "filter_regs":[

            ],
            "group_topic":"",
            "local_storage":true,
            "log_type":"plugin",
            "log_tz":"",
            "max_send_rate":-1,
            "merge_type":"topic",
            "plugin":{
                "inputs":[
                    {
                        "detail":{
                            "Detail":"\n# Read Redis's basic status information\n[[inputs.redis]]\n  interval = \"30s\"\n  ## specify servers via a url matching:\n  ##  [protocol://][:password]@address[:port]\n  ##  e.g.\n  ##    tcp://localhost:6379\n  ##    tcp://:password@192.168.99.100\n  ##\n  ## If no servers are specified, then localhost is used as the host.\n  ## If no port is specified, 6379 is used\n  ## servers = [\"tcp://host:port\"]\n  servers = [\"tcp://127.0.0.1:6379\"]\n  ## specify server password\n  # password = \"s#cr@t%\"\n  # password = \"\"\n\n  ## Optional TLS Config\n  # tls_ca = \"/etc/telegraf/ca.pem\"\n  # tls_cert = \"/etc/telegraf/cert.pem\"\n  # tls_key = \"/etc/telegraf/key.pem\"\n  ## Use TLS but skip chain & host verification\n  # insecure_skip_verify = true\n  [inputs.redis.tags]\n    __metricstore__ = \"ljp-test-bj_telegraf_test\"\n    cluster = \"test\"\n  \n    key = \"value\"\n  \n\n\n[[outputs.influxdb]]\n  ## The full HTTP or UDP URL for your InfluxDB instance.\n  ##\n  ## Multiple URLs can be specified for a single cluster, only ONE of the\n  ## urls will be written to each interval.\n  # urls = [\"unix:///var/run/influxdb.sock\"]\n  # urls = [\"udp://127.0.0.1:8089\"]\n  urls = [\"unix:///var/run/ljp-test-bj_telegraf_test.sock\"]\n  tagexclude = [\"__metricstore__\"]\n  skip_database_creation = true\n  [outputs.influxdb.tagpass]\n    __metricstore__ = [\"ljp-test-bj_telegraf_test\"]\n"
                        },
                        "type":"service_telegraf"
                    },
                    {
                        "detail":{
                            "Address":"unix:///var/run/ljp-test-bj_telegraf_test.sock",
                            "Format":"influx"
                        },
                        "type":"service_http_server"
                    }
                ],
                "flushers":[
                    {
                        "detail":{
                            "FileName":"./telegraf.log"
                        },
                        "type":"flusher_stdout"
                    }
                ]
            },
            "priority":0,
            "project_name":"",
            "raw_log":false,
            "region":"",
            "send_rate_expire":0,
            "sensitive_keys":[

            ],
            "shard_hash_key":[

            ],
            "tz_adjust":false,
            "version":1
        }
    }
}
```

6. 设置Telegraf

```
cd sys_conf_dir
# 下载Telegraf
wget https://logtail-release-cn-hangzhou.oss-cn-hangzhou.aliyuncs.com/linux64/telegraf/1.0.29/telegraf.tar.gz

$tar -xvf telegraf.tar.gz
telegraf/
telegraf/version
telegraf/javaagent/
telegraf/javaagent/jolokia-jvm.jar
telegraf/telegrafd
telegraf/arch_type
telegraf/conf.d/
telegraf/telegraf

chmod 755 telegraf/telegrafd
```

7. 启动redis 容器，并暴露6379端口。

```
docker pull redis
docker run -p 6379:6379 -d redis:latest redis-server
```

8. 启动logtail ，可以看到Telegraf 数据以及保存于telegraf.log 文件。

```
sudo ./ilogtail_1.0.29 --ilogtail_daemon_flag=false

2022-02-21 23:46:35 {"__name__":"redis:instantaneous_output_kbps","__labels__":"cluster#$#test|host#$#sls-backend-server-test011122076082.na131|key#$#value|port#$#6379|replication_role#$#master|server#$#127.0.0.1","__time_nano__":"1645458390000000000","__value__":"0","__time__":"1645458390"}
2022-02-21 23:46:35 {"__name__":"redis:sync_full","__labels__":"cluster#$#test|host#$#sls-backend-server-test011122076082.na131|key#$#value|port#$#6379|replication_role#$#master|server#$#127.0.0.1","__time_nano__":"1645458390000000000","__value__":"0","__time__":"1645458390"}
2022-02-21 23:46:35 {"__name__":"redis:mem_clients_slaves","__labels__":"cluster#$#test|host#$#sls-backend-server-test011122076082.na131|key#$#value|port#$#6379|replication_role#$#master|server#$#127.0.0.1","__time_nano__":"1645458390000000000","__value__":"0","__time__":"1645458390"}
2022-02-21 23:46:35 {"__name__":"redis:client_recent_max_output_buffer","__labels__":"cluster#$#test|host#$#sls-backend-server-test011122076082.na131|key#$#value|port#$#6379|replication_role#$#master|server#$#127.0.0.1","__time_nano__":"1645458390000000000","__value__":"0","__time__":"1645458390"}
```

## 日志服务采集Redis 指标数据实战

1. 参考[主机环境日志采集到SLS](https://github.com/alibaba/ilogtail/blob/main/docs/zh/usecases/How-to-setup-on-host.md) 建立主机iLogtail与阿里云日志服务的链接。
1. 创建MetricStore

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1645543648105-dd8004c3-f0f2-4f3c-a922-fc7376bfb32c.png#clientId=ud1ed2f8e-0c3c-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=901&id=u00e79867&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1802&originWidth=3582&originalType=binary&ratio=1&rotation=0&showTitle=false&size=1331965&status=done&style=none&taskId=u6bca1828-316a-44bf-8d79-94ec91483a6&title=&width=1791)

3. 在__创建机器组__页签中，创建机器组，然后在__机器组配置__页签中，应用机器组。选择一个机器组，将该机器组从__源机器组__移动到__应用机器组__。
3. 创建redis 采集配置，选择Redis监控。

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1645543646377-23d0cdc0-c42b-4205-b91c-a2f8bdbb6879.png#clientId=ud1ed2f8e-0c3c-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=840&id=u376e9200&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1680&originWidth=3582&originalType=binary&ratio=1&rotation=0&showTitle=false&size=1169149&status=done&style=none&taskId=u1cfd528e-f97a-42b5-b78c-020e8e5d919&title=&width=1791)

5. 配置Redis监控采集配置。

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1645543645507-6d15ae85-bafe-4df0-aa88-d9838f881daf.png#clientId=ud1ed2f8e-0c3c-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=786&id=u59ab1441&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1572&originWidth=2758&originalType=binary&ratio=1&rotation=0&showTitle=false&size=383606&status=done&style=none&taskId=u4f2d28e4-09b0-43e8-aef6-cd942a775f0&title=&width=1379)

4. 当采集配置成功后，此时可以打开可视化看板，查看展示redis 数据展示。

![image.png](https://cdn.nlark.com/yuque/0/2022/png/22279413/1645543648098-476ea724-8fb0-4e7b-aa06-96ec6182b58c.png#clientId=ud1ed2f8e-0c3c-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=747&id=ud075cad9&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1494&originWidth=2974&originalType=binary&ratio=1&rotation=0&showTitle=false&size=1090286&status=done&style=none&taskId=u3313b0bb-f694-4c77-badb-6998f854f2c&title=&width=1487)

## 总结

iLogtail 提供了完整的Telegraf 数据收集能力，无需改造迁移，即可将Telegraf采集转换为iLogtail 采集。而通过日志服务的MetricStore能力，构建了标准的PromQL Metrics 查询解决方案，让用户无需在多种Metrics 指标查询中选择。

## 参考文档

- [Telegraf 配置文档](https://github.com/influxdata/telegraf/blob/master/docs/CONFIGURATION.md)
- [接入Redis数据](https://help.aliyun.com/document_detail/185092.html)
- [iLogtail 的传输层协议](https://github.com/alibaba/ilogtail/blob/main/pkg/protocol/proto/sls_logs.proto)
- [主机环境日志采集到SLS](https://github.com/alibaba/ilogtail/blob/main/docs/zh/usecases/How-to-setup-on-host.md)
