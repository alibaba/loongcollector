# 系统参数

在`iLogtail`启动时，会加载`ilogtail_config.json`配置文件，该配置文件指定了`iLogtail`正常运行的一些基本配置项。

## 参数列表

| 参数                      | 类型     | 说明                                                                                                                                                                                                                                   |
| ----------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `cpu_usage_limit`       | double | <p>CPU使用阈值，以单核计算。取值范围：0.1~当前机器的CPU核心数</p><p><strong></strong></p><p><strong>警告</strong> `cpu_usage_limit`为软限制，实际`iLogtail`占用的CPU可能超过限制值，超限5分钟后将触发熔断保护，`iLogtail`自动重启。</p><p>例如设置为0.4，表示日志服务将尽可能限制 `iLogtail` 的CPU使用为CPU单核的40%，超出后 `iLogtail` 自动重启。</p> |
| `mem_usage_limit`       | Int    | <p>内存使用阈值。</p><p><strong>警告</strong> `mem_usage_limit`为软限制，实际`iLogtail`占用的内存可能超过限制值，超限5分钟后将触发熔断保护，Logtail自动重启。</p>                                                                                      |
| `default_access_key_id` | String | 写入 `SLS` 的 `access_id`，需要具备写入权限。                                                                                                                                                                                                                |
| `default_access_key`    | String | 写入 `SLS` 的 `access_key`，需要具备写入权限。                                                                                                                                                                                                                 |
| `config_scan_interval`    | Int | 本地配置热加载的更新间隔，单位为秒。 |
| `data_server_port`    | Int |<p>用于控制 `flusher_sls` 往 `SLS` 发送的协议类型。</p> <p>取值范围：443（默认），表示使用 `HTTPS` 协议发送；80表示使用 `HTTP` 协议发送。</p><p>如果使用`SLS`内网域名写入，建议使用`HTTP`协议发送，提高传输性能。</p> |
| `send_running_status`    | Bool | 为了更好的了解 `iLogtail` 的使用情况，以便做出更有针对性的发展规划，`iLogtail` 会上报一些脱敏后的运行统计信息。您也可以手动关闭此开关。                                              |
| `host_path_blacklist` | String | 全局主机路径黑名单，黑名单为子串匹配，Linux下多个子串以:分隔，Windows下以;分隔。比如禁止采集NAS挂载，可以配置为`/volumes/kubernetes.io~csi/nas-`。 |
| `metrics_report_method` | String | <p>自身指标输出方式。默认为空，即不输出指标。</p><p>当前支持的值：</br>`file`：每分钟将指标输出到`ilogtail`运行目录下的`self_metrics`目录，文件格式为`self-metrics-&{time}.json`，最多保留60个指标文件（即1小时的数据）。该方式适合本地调试使用。</p> |

### 典型配置

```json
{
    "default_access_key_id": "",
    "default_access_key": "",
    "cpu_usage_limit" : 0.4,
    "mem_usage_limit" : 384
}
```

## 环境变量

### containerd runtime相关环境变量配置

| 参数                      | 类型     | 说明                                                                                                         |
| ----------------------- |--------|------------------------------------------------------------------------------------------------------------|
| `USE_CONTAINERD`       | Bool   | 是否使用containerd runtime，非必选。ilogtail会自动通过接口探测。                                                              |
| `CONTAINERD_SOCK_PATH`       | String | 自定义containerd sock路径，非必选。默认为/run/containerd/containerd.sock。自定义取值可以通过查看/etc/containerd/config.toml grpc.address字段获取。 |
| `CONTAINERD_STATE_DIR` | String | 自定义containerd 数据目录，非必选。自定义取值可以通过查看/etc/containerd/config.toml state字段获取。                                             |
| `LOGTAIL_LOG_LEVEL` | String |  用于控制/apsara/sls/ilogtail和golang插件的日志等级，支持通用日志等级，如trace, debug，info，warning，error，fatal|

> 因为k8s本身自带资源限制的功能，所以如果你要将ilogtail部署到k8s中，可以通过将`cpu_usage_limit` 和 `mem_usage_limit` 设置为一个很大的值（比如99999999），以此来达到“关闭”ilogtail自身熔断功能的目的。
