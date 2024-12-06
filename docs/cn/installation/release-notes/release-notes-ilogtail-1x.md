# 发布历史(1.x版本)

## 1.8.4

### 发布记录

发版日期：2024 年 1 月 2 日

优化

* goprofile插件上报数据中使用机器的IP地址 [#1281](https://github.com/alibaba/ilogtail/pull/1281)

问题修复

* 修复正则配置time_key没有默认设置为time的兼容性问题 [#1272](https://github.com/alibaba/ilogtail/pull/1272)
* 修复使用libcurl因没有设置CURLOPT_NOSIGNAL导致偶尔崩溃的问题 [#1286](https://github.com/alibaba/ilogtail/pull/1286)
* 修复原生分隔符解析插件解析行首有空格的日志时字段错乱的问题 [#1288](https://github.com/alibaba/ilogtail/pull/1288)
* 修复原生插件丢弃超时日志时区处理错误的问题 [#1293](https://github.com/alibaba/ilogtail/pull/1293)
* 修复解析任意含有content key的json后，原生JSON插件总是错误保留原始content字段的问题 [#1295](https://github.com/alibaba/ilogtail/pull/1295)
* 修复原生分隔符插件的内存泄露问题 [#1299](https://github.com/alibaba/ilogtail/pull/1299)
* 修复因检查点转储早于目录注册导致的日志重复问题 [#1301](https://github.com/alibaba/ilogtail/pull/1301)
* 修复飞天日志无法解析带逗号时间格式的兼容性问题 [#1302](https://github.com/alibaba/ilogtail/pull/1302)
* 如果原生解析失败并选择保留原始日志，原始日志将仅保留在__raw_log__而不再保留在content字段以避免数据重复 [#1304](https://github.com/alibaba/ilogtail/pull/1304)
* 修复K8s集群Pod网络为HostNetwork时获取到的容器IP有时为空的问题 [#1280](https://github.com/alibaba/ilogtail/pull/1280)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.8.4.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.8.4.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.8.4/ilogtail-1.8.4.linux-amd64.tar.gz) | Linux | x86-64 | c4b079d01b8dd840f49d4d4387df24c268b4736995c2065a740edd9f2d727356 |
| [ilogtail-1.8.4.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.8.4/ilogtail-1.8.4.linux-arm64.tar.gz) | Linux | arm64  | c4c7bb1ccec34cf19424c70f3447121a2dd0285c428bc78308e76ca10348c025 |

### Docker 镜像

**Docker Pull 命令**&#x20;

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.8.4
```

## 1.8.3

### 发布记录

发版日期：2023 年 12 月 7 日

问题修复

* 修复因#1216引入的日志重复采集问题 [#1232](https://github.com/alibaba/ilogtail/pull/1232)
* 修复container info含nil字段导致的插件崩溃 [#1247](https://github.com/alibaba/ilogtail/pull/1247)
* 修复ProcessorParseDelimiterNative解析携带下一行数据的问题 [#1250](https://github.com/alibaba/ilogtail/pull/1250)
* 修复在反压情况下可能出现的文件无法读完的问题 [#1251](https://github.com/alibaba/ilogtail/pull/1251)
* 修复plugin_export panic导致插件崩溃的问题 [#1252](https://github.com/alibaba/ilogtail/pull/1252)
* 修复解析Apsara格式日志导致的崩溃问题 [#1253](https://github.com/alibaba/ilogtail/pull/1253)
* 修复解析Apsara格式日志解析数据黏连问题 [#1255](https://github.com/alibaba/ilogtail/pull/1255)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.8.3.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.8.3.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.8.3/ilogtail-1.8.3.linux-amd64.tar.gz) | Linux | x86-64 | 1cd352dec783247c4500074f77d8cfb88b607e28f6c95039c8f3da2a7b5880e3 |
| [ilogtail-1.8.3.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.8.3/ilogtail-1.8.3.linux-arm64.tar.gz) | Linux | arm64  | 6d77b86ed4b38605ed9e87b3d3ec049da5497b8c48e2cb9ec5334324ef26f0aa |

### Docker 镜像

**Docker Pull 命令**&#x20;

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.8.3
```

## 1.8.1

### 发布记录

发版日期：2023 年 11 月 8 日

问题修复

* 修复因字符串内存重用导致Promethus插件发送错误数据 [#1201](https://github.com/alibaba/ilogtail/pull/1201)
* 修复采集多个SkyWalking实例时丢失JVM指标的问题 [#1163](https://github.com/alibaba/ilogtail/pull/1163)
* 修复ElasticSearch Flusher TLS鉴权问题 [#1157](https://github.com/alibaba/ilogtail/issues/1157)
* 修复Profiling在相同Java/Go堆栈下类型错误问题 [#1187](https://github.com/alibaba/ilogtail/pull/1187)
* 修复从1.3之前的版本升级时本地send buffer丢失的问题 [#1199](https://github.com/alibaba/ilogtail/pull/1199)
* 修复strptime_ns解析%c与原有striptime存在差异的问题 [#1204](https://github.com/alibaba/ilogtail/pull/1204)
* 修复topic提取命名不支持带下划线的问题 [#1205](https://github.com/alibaba/ilogtail/pull/1205)
* 修复存在多个jmxfetch配置时jmxfetch状态异常 [#1210](https://github.com/alibaba/ilogtail/pull/1210)
* 修复被采集容器内存WSS增长的问题 [#1216](https://github.com/alibaba/ilogtail/pull/1216)
* 修复采集路径黑名单配置错误没有日志提示的问题 [#1218](https://github.com/alibaba/ilogtail/pull/1218)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.8.1.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.8.1.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.8.1/ilogtail-1.8.1.linux-amd64.tar.gz) | Linux | x86-64 | b659e711b1960db995787b306c9d87c615345df562affdaa1a090dad7cb453f4 |
| [ilogtail-1.8.1.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.8.1/ilogtail-1.8.1.linux-arm64.tar.gz) | Linux | arm64  | a44ef5a4affcbff27dd551c57224c8e8447be37ad6c36292b63790673bba0b7c |

### Docker 镜像

**Docker Pull 命令**&#x20;

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.8.1
```

## 1.8.0

### 发布记录

发版日期：2023 年 10 月 10 日

新功能

* C++流水线重构，处理流程插件化 [#995](https://github.com/alibaba/ilogtail/pull/995)
* 多行切分新增continue/end模式，支持多个模式组合 [#978](https://github.com/alibaba/ilogtail/pull/978)
* 支持超时切分行 [#1067](https://github.com/alibaba/ilogtail/pull/1067)
* SLS Flusher 支持纳秒级日志时间[#1097](https://github.com/alibaba/ilogtail/issues/1097)
* 新增全局主机路径黑名单 [#1073](https://github.com/alibaba/ilogtail/pull/1073)
* 新增trace解析插件`processor_otel_metric` [#1130](https://github.com/alibaba/ilogtail/pull/1130)
* 环境变量方式创建采集配置时添加资源标签 [#1147](https://github.com/alibaba/ilogtail/pull/1147)

优化

* 编译镜像GCC升级到9.3.1 [#1108](https://github.com/alibaba/ilogtail/pull/1108)
* Kafka V2插件支持写数据时增加header [#1065](https://github.com/alibaba/ilogtail/issues/1065)
* 缓存未构成完整行的日志，减少读文件系统调用 [#1142](https://github.com/alibaba/ilogtail/pull/1142)
* 支持使用环境变量控制日志打印级别 [#959](https://github.com/alibaba/ilogtail/issues/959)
* 支持flusher输出时使用扁平格式 [#1112](https://github.com/alibaba/ilogtail/pull/1112)
* skywalking插件支持捕获`db.connection_string`标签 [#1131](https://github.com/alibaba/ilogtail/pull/1131)
* 统一对metrics V1模型的格式化处理 [#1060](https://github.com/alibaba/ilogtail/pull/1060)
* 校验网卡IP以获取更精确地主机IP [#1019](https://github.com/alibaba/ilogtail/pull/1019)

问题修复

* 解决采集有挂载卷的statefulset漂移到不同节点时数据重复采集的问题 [#1081](https://github.com/alibaba/ilogtail/issues/1081)
* 修复json processor支持array解析后的兼容性问题 [#1088](https://github.com/alibaba/ilogtail/issues/1088)
* 修复空文件inode复用后导致采集到的日志文件名错误的问题 [#1102](https://github.com/alibaba/ilogtail/issues/1102)
* 修复因checkpoint重新打开文件导致容器无法退出的问题 [#1109](https://github.com/alibaba/ilogtail/pull/1109)
* 修复stdout文件路径为软链时无法采集容器stdout的问题 [#1037](https://github.com/alibaba/ilogtail/issues/1037)
* 修复飞天日志时间无法调整时区的问题 [#1123](https://github.com/alibaba/ilogtail/pull/1123)
* 修复json模式日志最后没有回车可能解析不正确的问题 [#1126](https://github.com/alibaba/ilogtail/issues/1126)
* 修复读取到的数据开头含有非法jon时json解析异常的问题 [#1161](https://github.com/alibaba/ilogtail/issues/1161)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.8.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.8.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.8.0/ilogtail-1.8.0.linux-amd64.tar.gz) | Linux | x86-64 | 90f60c372379311880d78a30609e76924f07d1fc8fdbcb944a78c01cc94de891 |
| [ilogtail-1.8.0.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.8.0/ilogtail-1.8.0.linux-arm64.tar.gz) | Linux | arm64  | 6737c2f59f4a462d98e46fe5dca8d471a6634531e2cd8a2feb4a6beaaff22d20 |

### Docker 镜像

**Docker Pull 命令**&#x20;

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.8.0
```

## 1.7.1

### 发布记录

发版日期：2023 年 8 月 20 日

新功能

* 新增命令执行结果采集插件 input_command [#925](https://github.com/alibaba/ilogtail/pull/925)
* 新增log转sls metric插件 processor_log_to_sls_metric [#955](https://github.com/alibaba/ilogtail/pull/955)
* 新增log转sls trace插件 processor_otel_trace [#1028](https://github.com/alibaba/ilogtail/pull/1028)
* Elasticsearch flusher支持动态索引 [#979](https://github.com/alibaba/ilogtail/pull/979)
* HTTP Server支持接受原始文本 [#976](https://github.com/alibaba/ilogtail/issues/976)
* Json processor支持解析数组 [#972](https://github.com/alibaba/ilogtail/pull/972)

优化

* Service_prometheus支持K8s中自动伸缩 [#932](https://github.com/alibaba/ilogtail/pull/932)
* containerd容器采集支持自定义rootfs路径和自动发现路径 [#985](https://github.com/alibaba/ilogtail/pull/985)
* 纳秒级高精度时间性能优化和windows支持 [#1026](https://github.com/alibaba/ilogtail/pull/1026)
* 避免文件30天无更新后写入导致的重复采集问题 [#1039](https://github.com/alibaba/ilogtail/pull/1039)
* 添加C++ Core UT流水线 [#951](https://github.com/alibaba/ilogtail/pull/951)

问题修复

* 修复 service_go_profile可能panic的问题 [#1036](https://github.com/alibaba/ilogtail/pull/1036)
* 修复zstd批量发送问题 [#1006](https://github.com/alibaba/ilogtail/pull/1006)
* 修复service_otlp插件无法退出的问题 [#1040](https://github.com/alibaba/ilogtail/pull/1040)
* 修复env创建配置时ttl值非法与空行为不一致的问题 [#1045](https://github.com/alibaba/ilogtail/issues/1045)
* 修复当只有容器删除事件时容器因fd所得无法退出的问题 [#986](https://github.com/alibaba/ilogtail/issues/986)
* 为system_v2插件采集disk metrics增加超时避免卡死 [#933](https://github.com/alibaba/ilogtail/pull/933)
* 修复profile中相同的容器内文件路径数据互相覆盖的问题 [#1034](https://github.com/alibaba/ilogtail/issues/1034)
* 修复apsara_log_conf.json文件存在时不创建snapshot目录的问题 [#944](https://github.com/alibaba/ilogtail/issues/944)
* 修复otlp flusher时间戳总为1970-01-01的问题 [#1031](https://github.com/alibaba/ilogtail/issues/1031)
* 修复config更新后config server下发为删除的问题 [#1023](https://github.com/alibaba/ilogtail/pull/1023)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.7.1.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.7.1.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.7.1/ilogtail-1.7.1.linux-amd64.tar.gz) | Linux | x86-64 | 13029900e0bdd4f6db858d97b31d30f1659145fc40bfceb97f7946f35c94c232 |
| [ilogtail-1.7.1.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.7.1/ilogtail-1.7.1.linux-arm64.tar.gz) | Linux | arm64  | fe5e2e2b69cb664ed0fc0578d1fdec95e94ab00711b547c7b90591f0a07853e1 |
| [ilogtail-1.7.1.windows-amd64.zip](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.7.1/ilogtail-1.7.1.windows-amd64.zip)   | Windows | x86-64 | 593f734035f4c808654b638492f1e0d79205b2080a8975305f7f86b988841166 |

### Docker 镜像

**Docker Pull 命令**&#x20;

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.7.1
```

## 1.7.0

### 发布记录

发版日期：2023 年 7 月 2 日

新功能

* 写入SLS支持纳秒级高精度时间 [#952](https://github.com/alibaba/ilogtail/pull/952)

优化

* Go版本升级到1.19 [#936](https://github.com/alibaba/ilogtail/issues/415)
* 为metric_system_v2收集磁盘指标添加超时，避免影响其他指标采集 [#933](https://github.com/alibaba/ilogtail/pull/933)
* 增加日志超长切分失败告警 [#915](https://github.com/alibaba/ilogtail/pull/915)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.7.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.7.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.7.0/ilogtail-1.7.0.linux-amd64.tar.gz) | Linux | x86-64 | d64061f9b5dc33e40f385c5514000157cc530c5b1f325afb3552267231334cd8 |
| [ilogtail-1.7.0.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.7.0/ilogtail-1.7.0.linux-arm64.tar.gz) | Linux | arm64  | 27f9a34df79869bffb1c1be639c3602f1bd021c068e97fc3e287ef9cbabb3834 |

### Docker 镜像

**Docker Pull 命令**&#x20;

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.7.0
```

## 1.6.0

### 发布记录

发版日期：2023 年 6 月 8 日

新功能

* 新增OpenTelemetry指标格式转换SLS指标格式，支持将service_otlp和service_http_server插件采集的OpenTelemetry指标数据写入SLS时序库 [#577](https://github.com/alibaba/ilogtail/issues/577)
* 新增InfluxDB格式DecodeV2解码方法 [#846](https://github.com/alibaba/ilogtail/pull/846)
* 纯Go运行模式支持文件输入到文件文件输出测试 [#869](https://github.com/alibaba/ilogtail/pull/869)
* `metric_meta_kubernetes`插件支持采集[kruise](github.com/openkruise/kruise) CRD元数据 [#878](https://github.com/alibaba/ilogtail/pull/878)
* ConfigServer UI支持和协议使用POST方式替代GET方式传递数据 [#894](https://github.com/alibaba/ilogtail/pull/894)
* `service-otlp-input`插件支持gzip解压缩 [#875](https://github.com/alibaba/ilogtail/issues/875)
* 支持Prometheus更多服务发现 [#890](https://github.com/alibaba/ilogtail/pull/890)

优化

* 优化`flusher_pulsar`、`flusher_kafka_v2`在静态topic下场景性能 [#824](https://github.com/alibaba/ilogtail/pull/824)
* 升级Kafka客户端sarama版本到1.38.1，并对Kafka相关插件进行匹配改造 [#600](https://github.com/alibaba/ilogtail/issues/600)
* 优化Prometheus协议解析性能约20% [#866](https://github.com/alibaba/ilogtail/pull/866)
* 优化`processor_desensitize`性能 20-80% [#981](https://github.com/alibaba/ilogtail/pull/891)
* SLS监控统计日志中增加ConfigName和Tag，以区分不同配置和容器 [#905](https://github.com/alibaba/ilogtail/pull/905) [#840](https://github.com/alibaba/ilogtail/pull/840)

问题修复

* 修复网络异常时，异常高频访问ECS Meta的问题 [851](https://github.com/alibaba/ilogtail/issues/851)
* 修复`metric_system_v2`插件在多分区情况下采集磁盘容量重复计算的问题 [#835](https://github.com/alibaba/ilogtail/pull/835)
* 修复会应用已退出容器env采集配置的问题 [#825](https://github.com/alibaba/ilogtail/pull/825)
* 修复Converter将日志转为JSON时，误将字符HTML转义的问题 [#857](https://github.com/alibaba/ilogtail/pull/857)
* 修复ConfigServer Bug [#876](https://github.com/alibaba/ilogtail/pull/876)
* 修复多个容器文件采集配置指向同一个目录时，容器无法退出的问题 [#872](https://github.com/alibaba/ilogtail/issues/872)
* 修复`processor_gotime`插件无法表达0时区的问题 [#829](https://github.com/alibaba/ilogtail/issues/829)
* 修复env采集配置缓存无法过期的问题 [#845](https://github.com/alibaba/ilogtail/issues/845)
* 修复多行正则加速模式下，读取到的起始行无法匹配时被抛弃的问题 [#902](https://github.com/alibaba/ilogtail/issues/902)
* 修复多行正则模式下，行尾多余回车的问题 [#896](https://github.com/alibaba/ilogtail/issues/896)
* 修复通过文件获取容器信息时，文件更新导致容器Label丢失的问题 [#906](https://github.com/alibaba/ilogtail/pull/906)
* 修复`processor_string_replace`正则不匹配时字段丢失问题 [#892](https://github.com/alibaba/ilogtail/pull/892)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.6.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.6.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.6.0/ilogtail-1.6.0.linux-amd64.tar.gz) | Linux | x86-64 | 392766fc6b2f96df2da4bbd1ffcfc48e2027fc4613138236c7df72dde84d077f |
| [ilogtail-1.6.0.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.6.0/ilogtail-1.6.0.linux-arm64.tar.gz) | Linux | arm64  | 50993809b1b741aad4779be7cce3798adb12c1aa8b455538129c15607e186642 |
| [ilogtail-1.6.0.windows-amd64.zip](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.6.0/ilogtail-1.6.0.windows-amd64.zip)   | Windows | x86-64 | f0da617f09ed1db2cd41135a581fddff64c0572a708fba0ae1b1fc27540b817f |

### Docker 镜像

**Docker Pull 命令**&#x20;

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.6.0
```

## 1.5.0

### 发布记录

发版日期：2023 年 5 月 1 日

新功能

* 新增插件Extension机制 [#648](https://github.com/alibaba/ilogtail/pull/648)
* flusher\_http插件通过Extension机制支持自定义鉴权、过滤、熔断 [#648](https://github.com/alibaba/ilogtail/pull/648)
* 新增Fusher插件: flusher\_loki [#259](https://github.com/alibaba/ilogtail/issues/259)
* 新增Processor插件: processor\_string\_replace [#757](https://github.com/alibaba/ilogtail/pull/757)
* 支持在C++加速模式下通过文件为数据自动添加Tag [#812](https://github.com/alibaba/ilogtail/issues/812)
* 为SLS Data Server Endpoint添加受控的切换策略 [#802](https://github.com/alibaba/ilogtail/issues/802)
* 支持iLogtail通过网络代理管控和发送数据 [#806](https://github.com/alibaba/ilogtail/issues/806)
* 添加Windows编译脚本支持编译x86和x86-64版本 [#327](https://github.com/alibaba/ilogtail/issues/327)
* Profiling功能支持goprofile拉取模式 [#730](https://github.com/alibaba/ilogtail/issues/730)
* V2流水线完整支持OTLP Log模型 [#779](https://github.com/alibaba/ilogtail/issues/779)

优化

* 默认打开enable\_env\_ref\_in\_config选项以支持配置中环境变量替换 [#744](https://github.com/alibaba/ilogtail/issues/744)
* processor\_split\_key\_value插件性能优化并增加多字符引用符 [#762](https://github.com/alibaba/ilogtail/issues/762)

问题修复

* 修复使用flusher\_kafka\_v2/flusher\_pulsar插件时，TagFieldsRename选项中部分Tag字段无法被重命名的问题 [#744](https://github.com/alibaba/ilogtail/issues/744)
* 修复创建query模式的logstore时，无法自动创建索引的问题 [#798](https://github.com/alibaba/ilogtail/issues/798)
* 修复processor\_filter\_key有多个key时即使匹配也过滤日志的问题 [#816](https://github.com/alibaba/ilogtail/issues/816)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.5.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.5.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.5.0/ilogtail-1.5.0.linux-amd64.tar.gz) | Linux | x86-64 | ccb7e637bc7edc4e9fe22ab3ac79cedee63d80678711e0efc77122387ec73882 |
| [ilogtail-1.5.0.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.5.0/ilogtail-1.5.0.linux-arm64.tar.gz) | Linux | arm64  | f1d7940e08ee51f2c66d963d91c16903658ab1e0fc856002351248c94d0e6b0a |
| [ilogtail-1.5.0.windows-amd64.zip](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.5.0/ilogtail-1.5.0.windows-amd64.zip)   | Windows | x86-64 | 7b3ccbfcca18e1ffeb2e5db06b32fbcac3e8b7ecf8113a7006891bbe3c1a5a84 |

### Docker 镜像

**Docker Pull 命令**&#x20;

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.5.0
```

## 1.4.0

### 发布记录

发版日期：2023 年 3 月 21 日

新功能

* 新增Pulsar输出插件flusher\_pulsar [#540](https://github.com/alibaba/ilogtail/issues/540)
* 新增ClickHouse输出插件flusher\_clickhouse [#112](https://github.com/alibaba/ilogtail/issues/112)
* 新增Elasticsearch输出插件flusher\_elasticsearch [#202](https://github.com/alibaba/ilogtail/issues/202)
* 新增Open Telemetry输入插件service\_otlp [#637](https://github.com/alibaba/ilogtail/issues/637)
* 新增使用plugins.yml裁剪插件和external\_plugins.yml引入外部插件库 [#625](https://github.com/alibaba/ilogtail/issues/625)
* 新增云主机实例元信息处理插件processor\_cloud\_meta [#692](https://github.com/alibaba/ilogtail/pull/692)
* 更新Open Telemetry输出插件新增Metric/Trace支持，同时重命名为flusher\_otlp [#646](https://github.com/alibaba/ilogtail/pull/646)
* 更新HTTP输入服务新增Pyroscope协议支持 [#653](https://github.com/alibaba/ilogtail/issues/653)

优化

* 更新Kafka V2 flusher支持TLS和Kerberos认证 [#601](https://github.com/alibaba/ilogtail/issues/601)
* 更新HTTP输出支持添加动态Header [#643](https://github.com/alibaba/ilogtail/pull/643)
* 更新通过ENV配置SLS Config新增冷存等Logstore参数支持 [#687](https://github.com/alibaba/ilogtail/issues/687)

问题修复

* 修复时区相关问题，使用系统时间和解析日志时间失败时忽略时区调整选项 [#550](https://github.com/alibaba/ilogtail/issues/550)
* 修复因inode复用导致的日志重复采集或文件名错乱问题 [#597](https://github.com/alibaba/ilogtail/issues/597)
* 修复Prometheus输入插件自动切换到streaming模式卡死的问题 [#684](https://github.com/alibaba/ilogtail/pull/684)
* 修复Grok插件解析中文会可能卡死的问题 [#644](https://github.com/alibaba/ilogtail/issues/644)
* 修复1.2.1版本中引入的容器发现内存使用过高的问题 [#661](https://github.com/alibaba/ilogtail/issues/661)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.4.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.4.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.4.0/ilogtail-1.4.0.linux-amd64.tar.gz) | Linux | x86-64 | d48fc6e8c76f117651487a33648ab6de0e2d8dd24ae399d9a7f534b81d639a61 |
| [ilogtail-1.4.0.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.4.0/ilogtail-1.4.0.linux-arm64.tar.gz) | Linux | arm64  | 1d488d0905e0fb89678e256c980e491e9c1c0d3ef579ecbbc18360afdcc1a853 |

### Docker 镜像

**Docker Pull 命令**&#x20;

```bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.4.0
```

## 1.3.1

### 发布记录

发版日期：2022 年 12 月 13 日

新功能

优化

问题修复

* 获取IP可能失败并引发崩溃的问题 [#576](https://github.com/alibaba/ilogtail/issues/576)
* ARM版本无法在Ubuntu 20.04上运行的问题 [#570](https://github.com/alibaba/ilogtail/issues/570)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.3.1.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.3.1.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.3.1/ilogtail-1.3.1.linux-amd64.tar.gz) | Linux | x86-64 | d74e2e8683fa9c01fcaa155e65953936c18611bcd068bcbeced84a1d7f170bd1 |
| [ilogtail-1.3.1.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.3.1/ilogtail-1.3.1.linux-arm64.tar.gz) | Linux | arm64  | d9a72b2ed836438b9d103d939d11cf1b0a73814e6bb3d0349dc0b6728b981eaf |

### Docker 镜像

**Docker Pull 命令**&#x20;

```bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.3.1
```

## 1.3.0

### 发布记录

发版日期：2022 年 11 月 30 日

新功能

* 新增Kafka新版本输出插件，支持自定义输出格式 [#218](https://github.com/alibaba/ilogtail/issues/218)
* 支持通过grpc输出Open Telemetry协议日志 [#418](https://github.com/alibaba/ilogtail/pull/418)
* 支持通过HTTP接入Open Telemetry协议日志 [#421](https://github.com/alibaba/ilogtail/issues/421)
* 新增脱敏插件processor\_desensitize [#525](https://github.com/alibaba/ilogtail/pull/525)
* 支持写入SLS时使用zstd压缩编码 [#526](https://github.com/alibaba/ilogtail/issues/526)

优化

* 减小发布二进制包大小 [#433](https://github.com/alibaba/ilogtail/pull/433)
* 使用ENV方式创建SLS资源时支持使用HTTPS协议 [#505](https://github.com/alibaba/ilogtail/issues/505)
* 支持创建SLS Logstore时选择query mode [#502](https://github.com/alibaba/ilogtail/issues/502)
* 使用插件处理时也支持输出内容在文件内偏移量 [#395](https://github.com/alibaba/ilogtail/issues/395)
* 默认支持采集容器标准输出时上下文保持连续 [#522](https://github.com/alibaba/ilogtail/pull/522)
* Prometheus数据接入内存优化 [#524](https://github.com/alibaba/ilogtail/pull/524)

问题修复

* 修复Docker环境下潜在的FD泄露和事件遗漏问题 [#529](https://github.com/alibaba/ilogtail/issues/529)
* 修复配置更新时文件句柄泄露的问题 [#420](https://github.com/alibaba/ilogtail/issues/420)
* IP在特殊主机名下解析错误 [#517](https://github.com/alibaba/ilogtail/pull/517)
* 修复多个配置路径存在父子目录关系时文件重复采集的问题 [#533](https://github.com/alibaba/ilogtail/issues/533)

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.3.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.3.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.3.0/ilogtail-1.3.0.linux-amd64.tar.gz) | Linux | x86-64 | 5ef5672a226089aa98dfb71dc48b01254b9b77c714466ecc1b6c4d9c0df60a50 |
| [ilogtail-1.3.0.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.3.0/ilogtail-1.3.0.linux-arm64.tar.gz) | Linux | arm64  | 73d33d4cac90543ea5c2481928e090955643c6dc6839535f53bfa54b6101704d |

### Docker 镜像

**Docker Pull 命令**&#x20;

```bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.3.0
```

## 1.2.1

### 发布记录

发版日期：2022 年 9 月 28 日

新功能

* 新增PostgreSQL采集插件。
* 新增SqlServer采集插件。
* 新增Grok处理插件。
* 支持JMX性能指标采集。
* 新增日志上下文聚合插件，可支持插件处理后的上下文浏览和日志topic提取。

优化

* 支持采集秒退容器的标准输出采集。
* 缩短已退出容器句柄释放时间。

问题修复

* 飞天日志格式微妙时间戳解析。

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.2.1.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.2.1.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.2.1/ilogtail-1.2.1.linux-amd64.tar.gz) | Linux | x86-64 | 8a5925f1bc265fd5f55614fcb7cebd571507c2c640814c308f62c498b021fe8f |
| [ilogtail-1.2.1.linux-arm64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.2.1/ilogtail-1.2.1.linux-arm64.tar.gz) | Linux | arm64  | 04be03f03eb722a3c9ba1b45b32c9be8c92cecabbe32bdc4672d229189e80c2f |

### Docker 镜像

**Docker Pull 命令**&#x20;

```bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.2.1
```

## 1.1.1

### 发布记录

发版日期：2022 年 8 月 22 日

新功能

* 新增Logtail CSV处理插件。
* 支持通过eBPF进行四层、七层网络流量分析，支持HTTP、MySQL、PgSQL、Redis、DNS协议。

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.1.1.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.1.1.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.1.1/ilogtail-1.1.1.linux-amd64.tar.gz) | Linux | x86-64 | 2330692006d151f4b83e4b8be0bfa6b68dc8d9a574c276c1beb6637c4a2939ec |

### Docker 镜像

**Docker Pull 命令**&#x20;

```bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.1.1
```

## 1.1.0

### 发布记录

发版日期：2022 年 6 月 29 日

新功能

* 开源C++核心模块代码
* netping插件支持httping和DNS解析耗时。

[详情和源代码](https://github.com/alibaba/ilogtail/blob/main/changes/v1.1.0.md)

### 下载

| 文件名                                                                                                                                          | 系统    | 架构     | SHA256 校验码                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------- | ----- | ------ | ---------------------------------------------------------------- |
| [ilogtail-1.1.0.linux-amd64.tar.gz](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/1.1.0/ilogtail-1.1.0.linux-amd64.tar.gz) | Linux | x86-64 | 2f4eadd92debe17aded998d09b6631db595f5f5aec9c8ed6001270b1932cad7d |

### Docker 镜像

**Docker Pull 命令**&#x20;

{% hint style="warning" %}
tag为1.1.0的镜像缺少必要环境变量，无法支持容器内文件采集和checkpoint，请使用1.1.0-k8s-patch或重新拉取latest
{% endhint %}

```bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:1.1.0-k8s-patch
```
