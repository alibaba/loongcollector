# 系统参数

LoongCollector 启动时会加载**实例级**系统参数配置文件，用于进程资源、credential、热加载间隔等行为。发行版默认路径为 `conf/instance_config/local/loongcollector_config.json`。目录布局参考文档：[目录结构说明](../installation/loongcollector-dir.md)。

为兼容历史环境，仍可通过环境变量指定该文件路径：`LOONG_CONFIG` 或 `ALIYUN_LOGTAIL_CONFIG`。若文档或安装包中仍出现 `ilogtail_config.json` 等文件名，多指同一类实例级配置，以实际部署包为准。

## 参数列表

| 参数                      | 类型     | 说明                                                                                                                                                                                                                                   |
| ----------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `cpu_usage_limit`       | double | <p>CPU使用阈值，以单核计算。取值范围：0.1~当前机器的CPU核心数</p><p><strong></strong></p><p><strong>警告</strong> `cpu_usage_limit`为软限制，实际 **LoongCollector** 进程占用的 CPU 可能超过限制值，超限 5 分钟后将触发熔断保护，**进程会自动重启**。</p><p>例如设置为 0.4，表示将尽可能限制 CPU 使用为单核的 40%，超出后触发上述保护逻辑。</p> |
| `mem_usage_limit`       | Int    | <p>内存使用阈值。</p><p><strong>警告</strong> `mem_usage_limit`为软限制，实际 **LoongCollector** 进程占用的内存可能超过限制值，超限 5 分钟后将触发熔断保护，**进程会自动重启**。</p>                                                                                      |
| `default_access_key_id` | String | 写入 `SLS` 的 `access_id`，需要具备写入权限。                                                                                                                                                                                                                |
| `default_access_key`    | String | 写入 `SLS` 的 `access_key`，需要具备写入权限。                                                                                                                                                                                                                 |
| `config_scan_interval`    | Int | **采集配置**等本地定义文件（`continuous_pipeline_config` / `onetime_pipeline_config` 等）热加载扫描间隔，单位为秒。 |
| `data_server_port`    | Int |<p>用于控制 `flusher_sls` 往 `SLS` 发送的协议类型。</p> <p>取值范围：443（默认），表示使用 `HTTPS` 协议发送；80表示使用 `HTTP` 协议发送。</p><p>如果使用`SLS`内网域名写入，建议使用`HTTP`协议发送，提高传输性能。</p> |
| `send_running_status`    | Bool | 为更好地了解 **LoongCollector** 的使用情况以便规划产品能力，进程会上报一些脱敏后的运行统计信息。您也可以手动关闭此开关。                                              |
| `host_path_blacklist` | String | 全局主机路径黑名单，黑名单为子串匹配，Linux下多个子串以:分隔，Windows下以;分隔。比如禁止采集NAS挂载，可以配置为`/volumes/kubernetes.io~csi/nas-`。 |
| `max_bytes_per_sec` | Int | 全局发送字节速率上限相关（与默认 `default_send_byte_per_sec` 等配合，详见代码）。 |
| `send_request_concurrency` | Int | 异步发送并发度（有上下限裁剪，见代码）。 |
| `process_thread_count` | Int | 处理线程数。 |
| `ebpf.receive_event_chan_cap` | Int | 用于接收内核事件的队列大小，默认为 4096 |
| `ebpf.admin_config.debug_mode` | Bool | 是否开启 ebpf debug 模式，默认为 false |
| `ebpf.admin_config.log_level` | String | ebpf 相关的日志级别，包括 info warn 和 debug，默认为 warn |
| `sls_request_dscp` | Int | 设置C++部分发送到SLS网络请求中的 DSCP 字段，取值范围为0到63。默认不设置。 |

### 典型配置

```json
{
    "default_access_key_id": "",
    "default_access_key": "",
    "cpu_usage_limit" : 0.4,
    "mem_usage_limit" : 384
}
```

```json
{
    "ebpf": {
        "receive_event_chan_cap": 4096,
        "admin_config": {
            "debug_mode": false,
            "log_level": "warn"
        }
    }
}
```

## 环境变量

### containerd runtime相关环境变量配置

| 参数                      | 类型     | 说明                                                                                                         |
| ----------------------- |--------|------------------------------------------------------------------------------------------------------------|
| `CONTAINERD_SOCK_PATH`       | String | 自定义containerd sock路径，非必选。默认为/run/containerd/containerd.sock。自定义取值可以通过查看/etc/containerd/config.toml grpc.address字段获取。 |
| `CONTAINERD_STATE_DIR` | String | 自定义containerd 数据目录，非必选。自定义取值可以通过查看/etc/containerd/config.toml state字段获取。                                             |
| `LOGTAIL_LOG_LEVEL` | String |  用于控制主进程（日志路径中可能仍包含历史分段名 `ilogtail`）与 Golang 插件的日志等级，支持 trace、debug、info、warning、error、fatal 等通用等级。|

> 因为 Kubernetes 本身自带资源限制能力，若将 **LoongCollector** 部署到集群中并希望主要依赖 Pod 的 CPU/内存配额而不使用本进程的软限制熔断，可将 `cpu_usage_limit` 与 `mem_usage_limit` 设为很大值（例如 `99999999`），以弱化实例级熔断触发。

未列入上表的实例级键仍可能通过配置合并、`ParseJsonToFlags` 映射到 gflags 或由专用加载逻辑解析，完整列表与语义以源码为准，可查阅 [AppConfig.cpp](https://github.com/alibaba/loongcollector/blob/main/core/app_config/AppConfig.cpp) 中的 `LoadResourceConf`、`LoadOtherConf`、`ParseJsonToFlags` 及各处的 `DEFINE_FLAG` 定义。
